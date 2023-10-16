// Copyright (c) Stanford University, The Regents of the University of
//               California, and others.
//
// All Rights Reserved.
//
// See Copyright-SimVascular.txt for additional details.
//
// Permission is hereby granted, free of charge, to any person obtaining
// a copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to
// permit persons to whom the Software is furnished to do so, subject
// to the following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
// IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
// TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
// PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER
// OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
// EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
// PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
// LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
// NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
// SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "BloodVessel.h"

void BloodVessel::setup_dofs(DOFHandler &dofhandler) {
  Block::setup_dofs_(dofhandler, 2, {});
}

void BloodVessel::update_constant(SparseSystem &system,
                                  std::vector<double> &parameters) {
  // Get parameters
  double capacitance = parameters[this->global_param_ids[ParamId::CAPACITANCE]];
  double inductance = parameters[this->global_param_ids[ParamId::INDUCTANCE]];

  // Set element contributions
  system.E.coeffRef(this->global_eqn_ids[0], this->global_var_ids[3]) =
      -inductance;
  system.E.coeffRef(this->global_eqn_ids[1], this->global_var_ids[0]) =
      -capacitance;
  system.F.coeffRef(this->global_eqn_ids[0], this->global_var_ids[0]) = 1.0;
  system.F.coeffRef(this->global_eqn_ids[0], this->global_var_ids[2]) = -1.0;
  system.F.coeffRef(this->global_eqn_ids[1], this->global_var_ids[1]) = 1.0;
  system.F.coeffRef(this->global_eqn_ids[1], this->global_var_ids[3]) = -1.0;
}

void BloodVessel::update_solution(
    SparseSystem &system, std::vector<double> &parameters,
    Eigen::Matrix<double, Eigen::Dynamic, 1> &y,
    Eigen::Matrix<double, Eigen::Dynamic, 1> &dy) {
  // Get parameters
  double resistance = parameters[this->global_param_ids[ParamId::RESISTANCE]];
  double capacitance = parameters[this->global_param_ids[ParamId::CAPACITANCE]];
  double stenosis_coeff =
      parameters[this->global_param_ids[ParamId::STENOSIS_COEFFICIENT]];
  double q_in = y[this->global_var_ids[1]];
  double dq_in = dy[this->global_var_ids[1]];
  double stenosis_resistance = stenosis_coeff * fabs(q_in);

  // Set element contributions
  system.E.coeffRef(this->global_eqn_ids[1], this->global_var_ids[1]) =
      capacitance * (resistance + 2.0 * stenosis_resistance);
  system.F.coeffRef(this->global_eqn_ids[0], this->global_var_ids[1]) =
      -resistance - stenosis_resistance;
  system.D.coeffRef(this->global_eqn_ids[0], this->global_var_ids[1]) =
      -stenosis_resistance;

  double sgn_q_in = (0.0 < q_in) - (q_in < 0.0);
  system.D.coeffRef(this->global_eqn_ids[1], this->global_var_ids[1]) =
      2.0 * capacitance * stenosis_coeff * sgn_q_in * dq_in;
}

void BloodVessel::update_gradient(
    Eigen::SparseMatrix<double> &jacobian,
    Eigen::Matrix<double, Eigen::Dynamic, 1> &residual,
    Eigen::Matrix<double, Eigen::Dynamic, 1> &alpha, std::vector<double> &y,
    std::vector<double> &dy) {
  auto y0 = y[this->global_var_ids[0]];
  auto y1 = y[this->global_var_ids[1]];
  auto y2 = y[this->global_var_ids[2]];
  auto y3 = y[this->global_var_ids[3]];

  auto dy0 = dy[this->global_var_ids[0]];
  auto dy1 = dy[this->global_var_ids[1]];
  auto dy3 = dy[this->global_var_ids[3]];

  auto resistance = alpha[this->global_param_ids[ParamId::RESISTANCE]];
  auto capacitance = alpha[this->global_param_ids[ParamId::CAPACITANCE]];
  auto inductance = alpha[this->global_param_ids[ParamId::INDUCTANCE]];
  double stenosis_coeff = 0.0;

  if (this->global_param_ids.size() > 3) {
    stenosis_coeff =
        alpha[this->global_param_ids[ParamId::STENOSIS_COEFFICIENT]];
  }
  auto stenosis_resistance = stenosis_coeff * fabs(y1);

  jacobian.coeffRef(this->global_eqn_ids[0], this->global_param_ids[0]) = -y1;
  jacobian.coeffRef(this->global_eqn_ids[0], this->global_param_ids[2]) = -dy3;

  if (this->global_param_ids.size() > 3) {
    jacobian.coeffRef(this->global_eqn_ids[0], this->global_param_ids[3]) =
        -fabs(y1) * y1;
  }

  jacobian.coeffRef(this->global_eqn_ids[1], this->global_param_ids[0]) =
      capacitance * dy1;
  jacobian.coeffRef(this->global_eqn_ids[1], this->global_param_ids[1]) =
      -dy0 + (resistance + 2 * stenosis_resistance) * dy1;

  if (this->global_param_ids.size() > 3) {
    jacobian.coeffRef(this->global_eqn_ids[1], this->global_param_ids[3]) =
        2.0 * capacitance * fabs(y1) * dy1;
  }

  residual(this->global_eqn_ids[0]) =
      y0 - (resistance + stenosis_resistance) * y1 - y2 - inductance * dy3;
  residual(this->global_eqn_ids[1]) =
      y1 - y3 - capacitance * dy0 +
      capacitance * (resistance + 2.0 * stenosis_resistance) * dy1;
}

std::map<std::string, int> BloodVessel::get_num_triplets() {
  return num_triplets;
}