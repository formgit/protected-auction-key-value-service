// Copyright 2023 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "components/internal_server/local_lookup.h"

#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "absl/log/log.h"
#include "components/data_server/cache/cache.h"
#include "components/internal_server/lookup.h"
#include "components/internal_server/lookup.pb.h"
#include "components/query/driver.h"
#include "components/query/scanner.h"
#include "src/cpp/telemetry/metrics_recorder.h"

namespace kv_server {
namespace {

using privacy_sandbox::server_common::MetricsRecorder;
using privacy_sandbox::server_common::ScopeLatencyRecorder;

constexpr char kKeySetNotFound[] = "KeysetNotFound";
constexpr char kLocalRunQuery[] = "LocalRunQuery";

class LocalLookup : public Lookup {
 public:
  explicit LocalLookup(const Cache& cache, MetricsRecorder& metrics_recorder)
      : cache_(cache), metrics_recorder_(metrics_recorder) {}

  absl::StatusOr<InternalLookupResponse> GetKeyValues(
      const RequestContext& request_context,
      const absl::flat_hash_set<std::string_view>& keys) const override {
    return ProcessKeys(request_context, keys);
  }

  absl::StatusOr<InternalLookupResponse> GetKeyValueSet(
      const RequestContext& request_context,
      const absl::flat_hash_set<std::string_view>& key_set) const override {
    return ProcessKeysetKeys(request_context, key_set);
  }

  absl::StatusOr<InternalRunQueryResponse> RunQuery(
      const RequestContext& request_context, std::string query) const override {
    return ProcessQuery(request_context, query);
  }

 private:
  InternalLookupResponse ProcessKeys(
      const RequestContext& request_context,
      const absl::flat_hash_set<std::string_view>& keys) const {
    InternalLookupResponse response;
    if (keys.empty()) {
      return response;
    }
    auto kv_pairs = cache_.GetKeyValuePairs(request_context, keys);

    for (const auto& key : keys) {
      SingleLookupResult result;
      const auto key_iter = kv_pairs.find(key);
      if (key_iter == kv_pairs.end()) {
        auto status = result.mutable_status();
        status->set_code(static_cast<int>(absl::StatusCode::kNotFound));
        status->set_message("Key not found");
      } else {
        result.set_value(std::move(key_iter->second));
      }
      (*response.mutable_kv_pairs())[key] = std::move(result);
    }
    return response;
  }

  absl::StatusOr<InternalLookupResponse> ProcessKeysetKeys(
      const RequestContext& request_context,
      const absl::flat_hash_set<std::string_view>& key_set) const {
    InternalLookupResponse response;
    if (key_set.empty()) {
      return response;
    }
    auto key_value_set_result = cache_.GetKeyValueSet(request_context, key_set);
    for (const auto& key : key_set) {
      SingleLookupResult result;
      const auto value_set = key_value_set_result->GetValueSet(key);
      if (value_set.empty()) {
        auto status = result.mutable_status();
        status->set_code(static_cast<int>(absl::StatusCode::kNotFound));
        status->set_message("Key not found");
        metrics_recorder_.IncrementEventCounter(kKeySetNotFound);
      } else {
        auto keyset_values = result.mutable_keyset_values();
        keyset_values->mutable_values()->Add(value_set.begin(),
                                             value_set.end());
      }
      (*response.mutable_kv_pairs())[key] = std::move(result);
    }
    return response;
  }

  absl::StatusOr<InternalRunQueryResponse> ProcessQuery(
      const RequestContext& request_context, std::string query) const {
    ScopeLatencyRecorder latency_recorder(std::string(kLocalRunQuery),
                                          metrics_recorder_);
    if (query.empty()) return absl::OkStatus();
    std::unique_ptr<GetKeyValueSetResult> get_key_value_set_result;
    kv_server::Driver driver([&get_key_value_set_result](std::string_view key) {
      return get_key_value_set_result->GetValueSet(key);
    });

    std::istringstream stream(query);
    kv_server::Scanner scanner(stream);
    kv_server::Parser parse(driver, scanner);
    int parse_result = parse();
    if (parse_result) {
      return absl::InvalidArgumentError("Parsing failure.");
    }
    get_key_value_set_result =
        cache_.GetKeyValueSet(request_context, driver.GetRootNode()->Keys());

    auto result = driver.GetResult();
    if (!result.ok()) {
      return result.status();
    }
    InternalRunQueryResponse response;
    response.mutable_elements()->Assign(result->begin(), result->end());
    return response;
  }

  const Cache& cache_;
  MetricsRecorder& metrics_recorder_;
};

}  // namespace

std::unique_ptr<Lookup> CreateLocalLookup(
    const Cache& cache,
    privacy_sandbox::server_common::MetricsRecorder& metrics_recorder) {
  return std::make_unique<LocalLookup>(cache, metrics_recorder);
}

}  // namespace kv_server
