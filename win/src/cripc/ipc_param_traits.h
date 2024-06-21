// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MINI_CHROMIUM_SRC_CRIPC_IPC_PARAM_TRAITS_H_
#define MINI_CHROMIUM_SRC_CRIPC_IPC_PARAM_TRAITS_H_

// Our IPC system uses the following partially specialized header to define how
// a data type is read, written and logged in the IPC system.

namespace cripc {

template <class P> struct ParamTraits {
};

template <class P>
struct SimilarTypeTraits {
  typedef P Type;
};

}  // namespace cripc

#endif  // MINI_CHROMIUM_SRC_CRIPC_IPC_PARAM_TRAITS_H_