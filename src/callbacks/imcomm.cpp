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
// imcomm .hpp .cpp - Send gradient updates between models
////////////////////////////////////////////////////////////////////////////////

#include "lbann/callbacks/imcomm.hpp"

#include "lbann/utils/exception.hpp"
#include "lbann/utils/timer.hpp"

#include <callbacks.pb.h>

#include <typeinfo>
#include <typeindex>

namespace lbann {
namespace callback {

imcomm::imcomm(imcomm::comm_type ct,
    const std::shared_ptr<lbann_summary>& summarizer) :
  m_default_ct(ct), m_summarizer(summarizer) {}

imcomm::imcomm(imcomm::comm_type ct,
    std::unordered_set<weights *> weights_list,
    const std::shared_ptr<lbann_summary>& summarizer) :
  imcomm(ct, summarizer) {
  for (weights *w : weights_list) {
    m_weights_params[w] = {};
    m_weights_params[w].ct = ct;
  }
}

void imcomm::set_weights_comm(weights *w,
                                             comm_type ct) {
  m_weights_params[w] = {};
  m_weights_params[w].ct = ct;
}

void imcomm::setup(model *m) {
  for (weights *w : m->get_weights()) {

    // Add weights if not already in list
    if (m_weights_params.find(w) == m_weights_params.end()) {
      m_weights_params[w] = {};
      m_weights_params[w].ct = (w->get_optimizer() != nullptr ?
                                m_default_ct :
                                NONE);
    }

    // Setup imcomm parameters if needed
    imcomm_params& params = m_weights_params[w];
    if (params.ct != NONE) {
      optimizer *opt = w->get_optimizer();
      if (opt == nullptr) {
        std::stringstream err;
        err << "imcomm: trying to do inter-model gradient communication on "
            << w->get_name() << ", which has no optimizer";
        LBANN_ERROR(err.str());
      }
    }

  }
}

void imcomm::on_train_begin(model *m) {
  lbann_comm *comm = m->get_comm();
  if (comm->get_num_trainers() == 1) {
    return;  // No point with only one model.
  }
  for (weights *w : m->get_weights()) {
    auto values = std::unique_ptr<AbsDistMat>{w->get_values().Copy()};
    comm->intertrainer_broadcast_matrix(*values, 0);
    w->set_values(*values);
  }
}

void imcomm::on_backward_prop_end(model *m) {
  lbann_comm *comm = m->get_comm();
  if (comm->get_num_trainers() == 1 ||
      m->get_execution_mode() != execution_mode::training) {
    return;  // No point with only one model.
  }
  for (weights *w : m->get_weights()) {
    EvalType start_time = get_time();
    imcomm_params& params = m_weights_params[w];
    if (params.ct == NONE) {
      continue;
    }
    optimizer *opt = w->get_optimizer();
    auto gradient = std::unique_ptr<AbsDistMat>{opt->get_gradient().Copy()};
    Mat* local_gradients = &(static_cast<CPUMat&>(gradient->Matrix()));
    switch (params.ct) {
    case NORMAL:
      comm->intertrainer_sum_matrix(*local_gradients);
      break;
    default:
      LBANN_ERROR("imcomm: unknown comm type");
    }
    opt->clear_gradient();
    opt->add_to_gradient(*gradient);
    EvalType im_time = get_time() - start_time;
    do_summary(m, w, im_time);
  }
}

void imcomm::do_summary(model *m, weights *w,
                                       EvalType im_time) {
  if (m_summarizer == nullptr) {
    return;
  }
  std::string prefix = w->get_name() + "/imcomm_";
  m_summarizer->reduce_scalar(prefix + "time",
                              im_time, m->get_step(execution_mode::training));
  // Use the same approximation the comm layer does.
  const CPUMat& local_gradients =
    static_cast<const CPUMat&>(w->get_optimizer()->get_gradient().LockedMatrix());
  size_t bytes_sent =
    sizeof(DataType) * local_gradients.Height() * local_gradients.Width();
  size_t bytes_received =
    sizeof(DataType) * local_gradients.Height() * local_gradients.Width();
  m_summarizer->reduce_scalar(prefix + "bytes_sent",
                              bytes_sent, m->get_step(execution_mode::training));
  m_summarizer->reduce_scalar(prefix + "bytes_received",
                              bytes_received, m->get_step(execution_mode::training));
}

static std::vector<std::string> comm_type_names  = { "none", "normal" };

/** returns a string representation of the weight_initialization */
std::string get_comm_type_name(imcomm::comm_type m) {
  if ((int)m < 0 or (int)m >= (int)comm_type_names.size()) {
    LBANN_ERROR(" Invalid comm_type");
  }
  return comm_type_names[(int)m];
}

std::unique_ptr<callback_base>
build_imcomm_callback_from_pbuf(
  const google::protobuf::Message& proto_msg,
  const std::shared_ptr<lbann_summary>& summarizer) {
  const auto& params = dynamic_cast<const lbann_data::Callback::CallbackImComm&>(proto_msg);
  const auto& type_str = params.intertrainer_comm_method();
  imcomm::comm_type type = imcomm::comm_type::NONE;
  if (type_str == "none") {
    type = imcomm::comm_type::NONE;
  } else if (type_str == "normal") {
    type = imcomm::comm_type::NORMAL;
  } else {
    std::ostringstream err;
    err << "invalid inter-model communication type (" << type_str << ")";
    LBANN_ERROR(err.str());
  }
  std::unordered_set<weights*> selected_weights; /// @todo Initialize weights
  return make_unique<imcomm>(type, selected_weights, summarizer);
}

} // namespace callback
} // namespace lbann
