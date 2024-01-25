/*
 * Copyright 2024 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef COMPONENTS_UTIL_REQUEST_CONTEXT_H_
#define COMPONENTS_UTIL_REQUEST_CONTEXT_H_

#include <memory>
#include <string>
#include <utility>

#include "components/telemetry/server_definition.h"

namespace kv_server {

// RequestContext holds the reference of metrics context that ties to a single
// request The request_id can be either passed from upper stream or assigned
// from uuid generated when RequestContext is constructed.

class RequestContext {
 public:
  explicit RequestContext(KVMetricsContext& metrics_context)
      : metrics_context_(metrics_context) {}
  KVMetricsContext& GetMetricsContext() const;
  ~RequestContext() = default;

 private:
  KVMetricsContext& metrics_context_;
};

}  // namespace kv_server

#endif  // COMPONENTS_UTIL_REQUEST_CONTEXT_H_
