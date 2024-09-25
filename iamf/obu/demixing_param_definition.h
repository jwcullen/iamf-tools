/*
 * Copyright (c) 2024, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 3-Clause Clear License
 * and the Alliance for Open Media Patent License 1.0. If the BSD 3-Clause Clear
 * License was not distributed with this source code in the LICENSE file, you
 * can obtain it at www.aomedia.org/license/software-license/bsd-3-c-c. If the
 * Alliance for Open Media Patent License 1.0 was not distributed with this
 * source code in the PATENTS file, you can obtain it at
 * www.aomedia.org/license/patent.
 */
#ifndef OBU_DEMIXING_PARAM_DEFINITION_H_
#define OBU_DEMIXING_PARAM_DEFINITION_H_

#include <memory>

#include "absl/status/status.h"
#include "iamf/common/read_bit_buffer.h"
#include "iamf/common/write_bit_buffer.h"
#include "iamf/obu/demixing_info_parameter_data.h"
#include "iamf/obu/param_definitions.h"

namespace iamf_tools {

/* !\brief Parameter definition for demixing info.
 */
class DemixingParamDefinition : public ParamDefinition {
 public:
  /*!\brief Default constructor.
   */
  DemixingParamDefinition() : ParamDefinition(kParameterDefinitionDemixing) {}

  /*!\brief Default destructor.
   */
  ~DemixingParamDefinition() override = default;

  friend bool operator==(const DemixingParamDefinition& lhs,
                         const DemixingParamDefinition& rhs) {
    return static_cast<const ParamDefinition&>(lhs) ==
           static_cast<const ParamDefinition&>(rhs);
  }

  /*!\brief Deep clones a `DemixingParamDefinition`.
   *
   * \return A deep clone of this param definition.
   */
  std::unique_ptr<ParamDefinition> Clone() override {
    return std::make_unique<DemixingParamDefinition>(*this);
  };

  /*!\brief Validates and writes to a buffer.
   *
   * \param wb Buffer to write to.
   * \return `absl::OkStatus()` if successful. A specific status on failure.
   */
  absl::Status ValidateAndWrite(WriteBitBuffer& wb) const override;

  /*!\brief Reads from a buffer and validates the resulting output.
   *
   * \param rb Buffer to read from.
   * \return `absl::OkStatus()` if successful. A specific status on failure.
   */
  absl::Status ReadAndValidate(ReadBitBuffer& rb) override;

  /*!\brief Prints the parameter definition.
   */
  void Print() const override;

  DefaultDemixingInfoParameterData default_demixing_info_parameter_data_;

 private:
  /*!\brief Validates the specific `ParamDefinition`s are equivalent.
   *
   * \param other `ParamDefinition` to compare.
   * \return `true` if equivalent. `false` otherwise.
   */
  bool EquivalentDerived(const ParamDefinition& other) const override {
    const auto& other_demixing =
        dynamic_cast<const DemixingParamDefinition&>(other);
    return default_demixing_info_parameter_data_ ==
           other_demixing.default_demixing_info_parameter_data_;
  }
};

}  // namespace iamf_tools

#endif  // OBU_DEMIXING_PARAM_DEFINITION_H_