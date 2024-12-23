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
#ifndef CLI_IAMF_ENCODER_H_
#define CLI_IAMF_ENCODER_H_

#include <cstdint>
#include <list>
#include <memory>
#include <optional>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/log/log.h"
#include "absl/status/status.h"
#include "iamf/cli/audio_element_with_data.h"
#include "iamf/cli/audio_frame_decoder.h"
#include "iamf/cli/audio_frame_with_data.h"
#include "iamf/cli/channel_label.h"
#include "iamf/cli/demixing_module.h"
#include "iamf/cli/global_timing_module.h"
#include "iamf/cli/parameter_block_with_data.h"
#include "iamf/cli/parameters_manager.h"
#include "iamf/cli/proto/test_vector_metadata.pb.h"
#include "iamf/cli/proto/user_metadata.pb.h"
#include "iamf/cli/proto_to_obu/audio_frame_generator.h"
#include "iamf/cli/proto_to_obu/parameter_block_generator.h"
#include "iamf/obu/codec_config.h"
#include "iamf/obu/ia_sequence_header.h"
#include "iamf/obu/mix_presentation.h"
#include "iamf/obu/param_definitions.h"
#include "iamf/obu/types.h"

namespace iamf_tools {

/*!\brief A class that encodes an IA Sequence and generates OBUs.
 *
 * Descriptor OBUs are generated once at the beginning, and data OBUs are
 * generated iteratively for each temporal unit (TU). The use pattern of this
 * class is:
 *   IamfEncoder encoder(...);  // Call constructor.
 *
 *   encoder.GenerateDescriptorObus(...);
 *
 *   while (encoder.GeneratingDataObus()) {
 *     // Prepare for the next temporal unit; clear state of the previous TU.
 *     encoder.BeginTemporalUnit();
 *
 *     // For all audio elements and labels corresponding to this temporal unit:
 *     for each audio element: {
 *       for each channel label from the current element {
 *         encoder.AddSamples(audio_element_id, label, samples);
 *       }
 *     }
 *
 *     // When all samples (for all temporal units) are added:
 *     if (done_receiving_all_audio) {
 *       encoder.FinalizeAddSamples();
 *     }
 *
 *     // For all parameter block metadata corresponding to this temporal unit:
 *     encoder.AddParameterBlockMetadata(...);
 *
 *     // Get OBUs for next encoded temporal unit.
 *     encoder.OutputTemporalUnit(...);
 *   }
 *
 * Note the timestamps corresponding to `AddSamples()` and
 * `AddParameterBlockMetadata()` might be different from that of the output
 * OBUs obtained in `OutputTemporalUnit()`, because some codecs introduce a
 * frame of delay. We thus distinguish the concepts of input and output
 * timestamps (`input_timestamp` and `output_timestamp`) in the code below.
 */
class IamfEncoder {
 public:
  /*!\brief Constructor.
   *
   * \param user_metadata Input user metadata describing the IAMF stream.
   */
  IamfEncoder(const iamf_tools_cli_proto::UserMetadata& user_metadata);

  /*!\brief Generates descriptor OBUs.
   *
   * \param ia_sequence_header_obu Generated IA Sequence Header OBU.
   * \param codec_config_obus Map of Codec Config ID to generated Codec Config
   *        OBUs.
   * \param audio_elements Map of Audio Element IDs to generated OBUs with data.
   * \param mix_presentation_obus List of generated Mix Presentation OBUs.
   * \return `absl::OkStatus()` if successful. A specific status on failure.
   */
  absl::Status GenerateDescriptorObus(
      std::optional<IASequenceHeaderObu>& ia_sequence_header_obu,
      absl::flat_hash_map<uint32_t, CodecConfigObu>& codec_config_obus,
      absl::flat_hash_map<DecodedUleb128, AudioElementWithData>& audio_elements,
      std::list<MixPresentationObu>& mix_presentation_obus);

  /*!\brief Returns whether this encoder is generating data OBUs.
   *
   * \return True if still generating data OBUs.
   */
  bool GeneratingDataObus() const;

  /*!\brief Clears the state, e.g. accumulated samples for next temporal unit.
   */
  void BeginTemporalUnit();

  /*!\brief Gets the input timestamp of the data OBU generation iteration.
   *
   * \param input_timestamp Result of input timestamp.
   * \return `absl::OkStatus()` if successful. A specific status on failure.
   */
  absl::Status GetInputTimestamp(int32_t& input_timestamp);

  /*!\brief Adds audio samples belonging to the same temporal unit.
   *
   * The best practice is to not call this function after
   * `FinalizeAddSamples()`. But it is OK if you do -- just that the added
   * samples will be ignored and not encoded.
   *
   * \param audio_element_id ID of the audio element to add samples to.
   * \param label Channel label to add samples to.
   * \param samples Audio samples to add.
   */
  void AddSamples(DecodedUleb128 audio_element_id, ChannelLabel::Label label,
                  const std::vector<InternalSampleType>& samples);

  /*!\brief Finalizes the process of adding samples.
   *
   * This will signal the underlying codecs to flush all remaining samples,
   * as well as trim samples from the end.
   */
  void FinalizeAddSamples();

  /*!\brief Adds parameter block metadata belonging to the same temporal unit.
   *
   * \param parameter_block_metadata Parameter block metadata to add.
   * \return `absl::OkStatus()` if successful. A specific status on failure.
   */
  absl::Status AddParameterBlockMetadata(
      const iamf_tools_cli_proto::ParameterBlockObuMetadata&
          parameter_block_metadata);

  // TODO(b/349271713): Remove the `id_to_labeled_frame` output argument
  //                    when the mix presentation finalizer is moved inside
  //                    this class.
  /*!\brief Outputs data OBUs corresponding to one temporal unit.
   *
   * \param audio_frames List of generated audio frames corresponding to this
   *        temporal unit.
   * \param parameter_blocks List of generated parameter block corresponding
   *        to this temporal unit.
   * \param id_to_labeled_frame Map of Audio Element IDs to labeld frames;
   *        which is a data structure storing samples.
   * \param output_timestamp Output timestamp of this temporal unit.
   * \return `absl::OkStatus()` if successful. A specific status on failure.
   */
  absl::Status OutputTemporalUnit(
      std::list<AudioFrameWithData>& audio_frames,
      std::list<ParameterBlockWithData>& parameter_blocks,
      IdLabeledFrameMap& id_to_labeled_frame, int32_t& output_timestamp);

 private:
  // Input user metadata describing the IAMF stream.
  iamf_tools_cli_proto::UserMetadata user_metadata_;

  // Mapping from parameter IDs to per-ID parameter metadata.
  absl::flat_hash_map<DecodedUleb128, PerIdParameterMetadata>
      parameter_id_to_metadata_;

  // Mapping from parameter IDs to param definitions.
  absl::flat_hash_map<DecodedUleb128, const ParamDefinition*>
      param_definitions_;

  // Saved parameter blocks generated in one iteration.
  std::list<ParameterBlockWithData> temp_mix_gain_parameter_blocks_;
  std::list<ParameterBlockWithData> temp_demixing_parameter_blocks_;
  std::list<ParameterBlockWithData> temp_recon_gain_parameter_blocks_;

  // Cached mapping from Audio Element ID to labeled samples added in the same
  // iteration.
  absl::flat_hash_map<DecodedUleb128, LabelSamplesMap> id_to_labeled_samples_;

  // Whether the `FinalizeAddSamples()` has been called.
  bool add_samples_finalized_;

  // Various generators and modules used when generating data OBUs iteratively.
  ParameterBlockGenerator parameter_block_generator_;
  std::unique_ptr<ParametersManager> parameters_manager_;
  DemixingModule demixing_module_;
  std::unique_ptr<AudioFrameGenerator> audio_frame_generator_;
  std::unique_ptr<AudioFrameDecoder> audio_frame_decoder_;
  GlobalTimingModule global_timing_module_;
};

}  // namespace iamf_tools

#endif  // CLI_IAMF_ENCODER_H_
