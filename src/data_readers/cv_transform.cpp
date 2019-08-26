////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2014-2019, Lawrence Livermore National Security, LLC.
// Produced at the Lawrence Livermore National Laboratory.
// Written by the LBANN Research Team (B. Van Essen, et al.) listed in
// the CONTRIBUTORS file. <lbann-dev@llnl.gov>
//
// LLNL-CODE-697807.
// All rights reserved.
//
// This file is part of LBANN: Livermore Big Artificial Neural Network
// Toolkit. For details, see http://software.llnl.gov/LBANN or
// https://github.com/LLNL/LBANN.
//
// Licensed under the Apache License, Version 2.0 (the "Licensee"); you
// may not use this file except in compliance with the License.  You may
// obtain a copy of the License at:
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or
// implied. See the License for the specific language governing
// permissions and limitations under the license.
//
// cv_transform .cpp .hpp - base class for the transformation
//                          on image data in opencv format
////////////////////////////////////////////////////////////////////////////////

#include "lbann/data_readers/cv_transform.hpp"

#ifdef LBANN_HAS_OPENCV
namespace lbann {

const constexpr char* const cv_transform::cv_flip_desc[];

/** The mathematical constant (this is the way to get it in C++). */
const float cv_transform::pi = static_cast<float>(std::acos(-1));

double get_depth_normalizing_factor(const int cv_depth) {
  _SWITCH_CV_FUNC_1PARAM(cv_depth, depth_norm_factor, );
  // The caller must check the exception by detecting 0.0
  return 0.0;
}

double get_depth_denormalizing_factor(const int cv_depth) {
  _SWITCH_CV_FUNC_1PARAM(cv_depth, depth_norm_inverse_factor, );
  // The caller must check the exception by detecting 0.0
  return 0.0;
}

} // end of namespace lbann

#endif // LBANN_HAS_OPENCV