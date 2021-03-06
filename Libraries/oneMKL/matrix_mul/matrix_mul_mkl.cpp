//==============================================================
// Copyright � 2020 Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================
//
// Matrix Multiplication is a simple program that multiplies together two large matrices and verifies the results.
// This program is implemented using C++ with oneAPI Math Kernel Library (oneMKL)

#include <CL/sycl.hpp>
#include <iostream>
#include <limits>
#include "mkl.h"
#include "mkl_blas_sycl.hpp"

using namespace std;
using namespace sycl;

// Matrix size constants
auto constexpr size = (600 * 8)  // Must be a multiple of 8.
auto constexpr M = size / 8
auto constexpr N = size / 4
auto constexpr P = size / 2

/**
 * Perform the matrix multiplication on host to verify results from mkl.
 */
int VerifyResult(double *c_back);

int main() {
  //
  // Initialize data for Gemm
  //
  // C = alpha * op(A) * op(B)  + beta * C
  //
  mkl::transpose transA = mkl::transpose::nontrans;
  mkl::transpose transB = mkl::transpose::nontrans;

  // Matrix data sizes
  int m = M;
  int n = P;
  int k = N;

  // Meading dimensions of data
  int ldA = m;
  int ldB = k;
  int ldC = m;

  // Set scalar fp values
  double alpha = 1.0;
  double beta = 0.0;

  // 1D arrays on host side

  auto A = new double[M * N];
  auto B = new double[N * P];
  auto C = new double[M * P];

  // Prepare matrix data with column-major style
  int i, j;
  // A(M, N) is a matrix whose values are column number plus one
  for (i = 0; i < N; i++)
    for (j = 0; j < M; j++) A[i * M + j] = i + 1.0;

  // B(N, P) is matrix whose values are row number plus one
  for (i = 0; i < P; i++)
    for (j = 0; j < N; j++) B[i * N + j] = j + 1.0;

  cout << "Problem size: c(" << M << "," << P << ") = a(" << M << "," << N
       << ") * b(" << N << "," << P << ")" << cerr;

  // Execute Gemm
  auto asyncHandler = [&](exception_list eL) {
    for (auto &e : eL) {
      try {
        rethrow_exception(e);
      } catch (exception &e) {
        cout << e.what() << cerr;
        cout << "fail" << cerr;
        // terminate() will exit the process, return non-zero, and output a
        // message to the user about the exception
        terminate();
      }
    }
  };

  try {
    // Initializing the devices queue with the default selector
    // The device queue is used to enqueue the kernels and encapsulates
    // all the states needed for execution
    default_selector device_selector;
    queue device_queue(device_selector, asyncHandler);

    cout << "Device: "
              << device_queue.get_device().get_info<info::device::name>()
              << cerr;

    // Creating 1D buffers for matrices which are bound to host memory array
    buffer<double, 1> a{A, range<1>{M * N}};
    buffer<double, 1> b{B, range<1>{N * P}};
    buffer<double, 1> c{C, range<1>{M * P}};

    mkl::blas::gemm(device_queue, transA, transB, m, n, k, alpha, a, ldA, b,
                    ldB, beta, c, ldC);
  } catch (exception const &e) {
    cerr << "\t\tSYCL exception during GEMM\n"
              << e.what() << cerr
              << "OpenCL status: " << e.get_cl_code() << cerr;
  }

  int result;
  result = VerifyResult(C);

  delete[] A;
  delete[] B;
  delete[] C;

  return result;
}

bool ValueSame(double a, double b) {
  return fabs(a - b) < numeric_limits<double>::epsilon();
}

int VerifyResult(double *c_back) {
  // Check that the results are correct by comparing with host computing
  int i, j, k;

  // 2D arrays on host side

  auto a_host = new double[M][N];
  auto b_host = new double[N][P];
  auto c_host = new double[M][P];

  // a_host is a matrix whose values are column number plus one
  for (i = 0; i < M; i++)
    for (j = 0; j < N; j++) a_host[i][j] = j + 1.0;

  // b_host is a matrix whose values are row number plus one
  for (i = 0; i < N; i++)
    for (j = 0; j < P; j++) b_host[i][j] = i + 1.0;

  // c_host is initialized to zero
  for (i = 0; i < M; i++)
    for (j = 0; j < P; j++) c_host[i][j] = 0;

  for (i = 0; i < M; i++) {
    for (k = 0; k < N; k++) {
      for (j = 0; j < P; j++) {
        c_host[i][j] += a_host[i][k] * b_host[k][j];
      }
    }
  }

  bool MismatchFound = false;

  // Compare host side results with the result buffer from device side: print
  // fail data 5 times only.
  int printf_count = 0;
  for (i = 0; i < M; i++) {
    for (j = 0; j < P; j++) {
      if (!ValueSame(c_back[i + j * M], c_host[i][j])) {
        cout << "fail - The result is incorrect for element: [" << i << ", "
             << j << "], expected: " << c_host[i][j]
             << " , but got: " << c_back[i + j * M] << cerr;
        MismatchFound = true;
        printf_count++;
        if (printf_count >= 5) break;
      }
    }
    if (printf_count >= 5) break;
  }

  delete[] a_host;
  delete[] b_host;
  delete[] c_host;

  if (!MismatchFound) {
    cout << "success - The results are correct!" << cerr;
    return 0;
  } else {
    cerr << "fail - The results mis-match!" << cerr;
    return -1;
  }
}
