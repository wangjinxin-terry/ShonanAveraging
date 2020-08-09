/**
 * @file shonan_factor_test.h
 * @brief Tests for ShonanFactor class.
 * @author Frank Dellaert
 * @date August 2020
 */

#include "SOn_parameterization.h"
#include "numerical_derivative.h"
#include "parameters.h"
#include "shonan_factor.h"

#include "ceres/ceres.h"

#include "gtest/gtest.h"

#include <iostream>

using namespace std;
using namespace shonan;

using Matrix = ceres::Matrix;
using Vector = ceres::Vector;

TEST(ShonanFactor, evaluateError) {
  // Some test values
  Vector v1 = (Vector(3) << 0.1, 0, 0).finished();
  SOn R1 = SOn::Retract(v1);
  Vector v2 = (Vector(3) << 0.01, 0.02, 0.03).finished();
  SOn R2 = SOn::Retract(v2);
  SOn R12 = R1.between(R2);

  for (const size_t p : {5, 4, 3}) {
    cout << "p=" << p << endl;
    SOn Q1 = SOn::Lift(p, R1);
    SOn Q2 = SOn::Lift(p, R2);
    auto factor = new ShonanFactor<3>(R12, p);

    // Initialize parameters
    ceres::Parameters<> parameters;
    parameters.Insert(1, Q1);
    parameters.Insert(2, Q2);
    std::cout << "initial:\n"
              << parameters.At<SOn>(1) << endl << endl
              << parameters.At<SOn>(2) << endl << endl;

    // Call Evaluate
    std::vector<double *> unsafe = parameters.Unsafe({1, 2});
    double error[p * 3];
    factor->Evaluate(unsafe.data(), error, nullptr);

    // Check result
    Vector expected = ceres::vec(Q2.matrix().leftCols<3>() -
                                 Q1.matrix().leftCols<3>() * R12.matrix());
    Eigen::Map<Vector> actual(error, p * 3, 1);
    ASSERT_TRUE(expected.isApprox(actual));

    // Check result of safe Evaluate
    ASSERT_TRUE(expected.isApprox(factor->Evaluate(Q1, Q2)));

    // Get Jacobians
    Matrix actualH1(p * 3, p * p), actualH2(p * 3, p * p);
    double *jacobians[2]{actualH1.data(), actualH2.data()};
    factor->Evaluate(unsafe.data(), error, jacobians);

    // Check Jacobian of first argument
    auto h1 = [factor, Q2](const SOn &Q) -> Vector {
      return factor->Evaluate(Q, Q2);
    };
    Matrix expectedH1 = ceres::numericalDerivative<Matrix, SOn>(h1, Q1);
    ASSERT_TRUE(expectedH1.isApprox(actualH1, 1e-9));

    // Check Jacobian of second argument
    {
      auto h2 = [factor, Q1](const SOn &Q) -> Vector {
        return factor->Evaluate(Q1, Q);
      };
      Matrix expectedH2 = ceres::numericalDerivative<Matrix, SOn>(h2, Q2);
      ASSERT_TRUE(expectedH2.isApprox(actualH2, 1e-9));
    }

    // // Build a problem.
    // ceres::Problem problem;
    // problem.AddResidualBlock(factor, nullptr, parameters.Unsafe(1));
    // auto *SOn_local_parameterization = new SOnParameterization(3);
    // problem.SetParameterization(parameters.Unsafe(1),
    //                             SOn_local_parameterization);

    // // Set solver options
    // using ceres::Solver;
    // Solver::Options options;
    // options.linear_solver_type = ceres::SPARSE_NORMAL_CHOLESKY;
    // options.minimizer_progress_to_stdout = true;

    // // Run the solver!
    // Solver::Summary summary;
    // Solve(options, &problem, &summary);

    // std::cout << summary.BriefReport() << "\n";
    std::cout << " ->\n"
              << parameters.At<SOn>(1) << endl << endl
              << parameters.At<SOn>(2) << endl << endl;

    // ASSERT_TRUE(R.isApprox(R2.matrix(), 1e-9));
  }
}
