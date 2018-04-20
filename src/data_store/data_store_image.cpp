////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2014-2016, Lawrence Livermore National Security, LLC.
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
////////////////////////////////////////////////////////////////////////////////

#include "lbann/data_store/data_store_image.hpp"
#include "lbann/utils/exception.hpp"
#include "lbann/data_readers/data_reader.hpp"
#include "lbann/utils/timer.hpp"

namespace lbann {

data_store_image::~data_store_image() {
}

void data_store_image::setup() {

  if (m_master) std::cerr << "starting data_store_image::setup(); calling generic_data_store::setup()\n";
  generic_data_store::setup();

  set_name("data_store_image");

  //@todo needs to be designed and implemented!
  if (! m_in_memory) {
    LBANN_ERROR("not yet implemented");
  } 
  
  else {
    if (m_master) std::cerr << "calling get_minibatch_index_vector\n";
    get_minibatch_index_vector();

    if (m_master) std::cerr << "calling exchange_mb_indices\n";
    exchange_mb_indices();

    if (m_master) std::cerr << "calling get_my_datastore_indices\n";
    get_my_datastore_indices();

    if (m_master) std::cerr << "calling get_file_sizes\n";
    double tma = get_time();
    get_file_sizes();
    if (m_master) std::cerr << "get_file_sizes time: " << get_time() - tma << "\n";

    if (m_master) std::cerr << "calling allocate_memory\n";
    allocate_memory();

    if (m_master) std::cerr << "calling read_files\n";
    tma = get_time();
    read_files();
    if (m_master) std::cerr << "read_files time: " << get_time() - tma << "\n";

    if (m_master) std::cerr << "calling exchange_data\n";
    exchange_data();

    if (m_extended_testing) {
      if (m_master) std::cerr << "calling extended_testing\n";
      extended_testing();
    }
  }
}


void data_store_image::get_data_buf(int data_id, std::vector<unsigned char> *&buf, int multi_idx) {
  std::stringstream err;
  int index = data_id * m_num_img_srcs + multi_idx;
  if (m_my_minibatch_data.find(index) == m_my_minibatch_data.end()) {
    err << __FILE__ << " " << __LINE__ << " :: "
        << "failed to find index: " << index << " m_num_img_srcs: " << m_num_img_srcs
        << " multi_idx: " << multi_idx << " in m_my_data_hash; role: "
        << m_reader->get_role();
    throw lbann_exception(err.str());
  }

  buf = &m_my_minibatch_data[index];
}

void data_store_image::allocate_memory() {
  size_t m = 0;
  for (auto t : m_my_datastore_indices) {
    for (size_t i=0; i<m_num_img_srcs; i++) {
      m += m_file_sizes[t*m_num_img_srcs+i];
    }    
  }    
  m_data.resize(m);
}

void data_store_image::load_file(const std::string &dir, const std::string &fn, unsigned char *p, size_t sz) {
  std::string imagepath;
  if (dir != "") {
    imagepath = dir + fn;
  } else {
    imagepath = fn;
  }
  std::ifstream in(imagepath.c_str(), std::ios::in | std::ios::binary);
  if (!in) {
    std::stringstream err;
    err << __FILE__ << " " << __LINE__ << " :: "
        << "failed to open " << imagepath << " for reading"
        << "; dir: " << dir << "  fn: " << fn;
    throw lbann_exception(err.str());
  }
  in.read((char*)p, sz);
  if ((int)sz != in.gcount()) {
    std::stringstream err;
    err << __FILE__ << " " << __LINE__ << " :: "
        << "failed to read " << sz << " bytes from " << imagepath
        << " num bytes read: " << in.gcount();
    throw lbann_exception(err.str());
  }
  in.close();
}

void data_store_image::exchange_data() {
  if (m_master) std::cerr << "starting exchange_data\n";
  std::stringstream err;

  //build map: proc -> global indices that proc needs for this epoch, and
  //                   which I own
  std::unordered_map<int, std::unordered_set<int>> proc_to_indices;
  for (size_t p=0; p<m_all_minibatch_indices.size(); p++) {
    for (auto idx : m_all_minibatch_indices[p]) {
      int index = (*m_shuffled_indices)[idx];
      if (m_my_datastore_indices.find(index) != m_my_datastore_indices.end()) {
        proc_to_indices[p].insert(index);
      }
    }
  }

  //start sends
  std::vector<std::vector<El::mpi::Request<unsigned char>>> send_req(m_np);
  for (int p=0; p<m_np; p++) {
    send_req[p].resize(proc_to_indices[p].size()*m_num_img_srcs);
    size_t jj = 0;
    for (auto idx : proc_to_indices[p]) {
      for (size_t k=0; k<m_num_img_srcs; k++) {
        int index = idx*m_num_img_srcs+k;
        if (m_file_sizes.find(index) == m_file_sizes.end()) {
          err << __FILE__ << " " << __LINE__ << " :: "
              << " m_file_sizes.find(" << index << ") failed";
          throw lbann_exception(err.str());
        }
        if (m_offsets.find(index) == m_offsets.end()) {
          err << __FILE__ << " " << __LINE__ << " :: "
              << " m_offets.find(" << index << ") failed";
          throw lbann_exception(err.str());
        }
        int len = m_file_sizes[index];
        size_t offset = m_offsets[index];
        m_comm->nb_send<unsigned char>(m_data.data()+offset, len, 0, p, send_req[p][jj++]);

      }
    }
    if (jj != send_req[p].size()) throw lbann_exception("ERROR 1");
  } //start sends


  //build map: proc -> global indices that proc owns that I need
  proc_to_indices.clear();
  //note: datastore indices are global; no need to consult shuffled_indices
  for (auto idx : m_my_minibatch_indices_v) {
    int index = (*m_shuffled_indices)[idx];
    int owner = get_index_owner(index);
    proc_to_indices[owner].insert(index);
  }
  
  //start recvs
  m_my_minibatch_data.clear();
  std::vector<std::vector<El::mpi::Request<unsigned char>>> recv_req(m_np);
  for (auto t : proc_to_indices) {
    int owner = t.first;
    size_t jj = 0;
    const std::unordered_set<int> &s = t.second;
    recv_req[owner].resize(s.size()*m_num_img_srcs);
    for (auto idx : s) {
      for (size_t k=0; k<m_num_img_srcs; k++) {
        size_t index = idx*m_num_img_srcs+k;
        if (m_file_sizes.find(index) == m_file_sizes.end()) {
          err << __FILE__ << " " << __LINE__ << " :: "
              << " m_file_sizes.find(" << index << ") failed"
              << " m_file_sizes.size(): " << m_file_sizes.size()
              << " m_my_minibatch_indices_v.size(): " << m_my_minibatch_indices_v.size();
        }
        size_t len = m_file_sizes[index];
        m_my_minibatch_data[index].resize(len);
        m_comm->nb_recv<unsigned char>(m_my_minibatch_data[index].data(), len, 0, owner, recv_req[owner][jj++]);
      }
    }
  }

  //wait for sends to finish
  for (size_t i=0; i<send_req.size(); i++) {
    m_comm->wait_all<unsigned char>(send_req[i]);
  }

  //wait for recvs to finish
  for (size_t i=0; i<recv_req.size(); i++) {
    m_comm->wait_all<unsigned char>(recv_req[i]);
  }
}


void data_store_image::exchange_file_sizes(
  std::vector<int> &global_indices,
  std::vector<int> &bytes,
  std::vector<size_t> &offsets,
  int num_global_indices) {

  std::vector<int> num_bytes(m_np);
  int nbytes = global_indices.size();
  if (m_master) {
    m_comm->model_gather<int>(nbytes, num_bytes.data());
  } else {
    m_comm->model_gather<int>(nbytes, 0);
  }

  std::vector<int> disp(m_num_readers); 
  disp[0] = 0;
  for (int h=1; h<(int)m_num_readers; h++) {
    disp[h] = disp[h-1] + num_bytes[h-1];
  }
  std::vector<int> all_global_indices(num_global_indices);
  std::vector<int> all_bytes(num_global_indices);
  std::vector<size_t> all_offsets(num_global_indices);

  m_comm->all_gather<int>(global_indices, all_global_indices, bytes, disp, m_comm->get_world_comm());
  m_comm->all_gather<int>(num_bytes, all_bytes, num_bytes, disp, m_comm->get_world_comm());
  m_comm->all_gather<size_t>(offsets, all_offsets, num_bytes, disp, m_comm->get_world_comm());

  for (size_t j=0; j<all_global_indices.size(); j++) {
    m_file_sizes[all_global_indices[j]] = all_bytes[j];
    m_offsets[all_global_indices[j]] = all_offsets[j];
    ++j;
  }
}

}  // namespace lbann
