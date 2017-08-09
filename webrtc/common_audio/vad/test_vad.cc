#include <array>
#include <cmath>
#include <memory>

#include "gflags/gflags.h"
#include "webrtc/common_audio/vad/include/vad.h"
#include "webrtc/common_audio/wav_file.h"
#include "webrtc/rtc_base/logging.h"

namespace webrtc {
namespace test {
namespace {

constexpr size_t kAudioFrameLen = 480;
constexpr double kPi = 3.1415926535897;

DEFINE_string(i, "", "Input wav file");
DEFINE_string(o, "", "Output wav file");

int main(int argc, char* argv[]) {
  google::ParseCommandLineFlags(&argc, &argv, true);

  WavReader wav_reader(FLAGS_i);
  if (wav_reader.sample_rate() != 48000) {
    LOG(LS_ERROR) << "Unsupported sample rate";
    return 1;
  }
  if (wav_reader.num_channels() != 1) {
    LOG(LS_ERROR) << "Only mono wav files supported";
    return 2;
  }

  WavWriter wav_writer(FLAGS_o, wav_reader.sample_rate(), 1);
  std::unique_ptr<Vad> vad = CreateVad(Vad::Aggressiveness::kVadNormal);

  std::array<int16_t, kAudioFrameLen> samples;
  while (true) {
    const auto read_samples =
        wav_reader.ReadSamples(kAudioFrameLen, samples.data());
    if (read_samples < kAudioFrameLen)
      break;
    const auto is_speech = vad->VoiceActivity(samples.data(), kAudioFrameLen,
                                              wav_reader.sample_rate());

    if (!is_speech) {
      // Add beep sound.
      for (size_t i = 0; i < kAudioFrameLen; ++i) {
        const int16_t beep_sample =
            (32767 >> 2) * std::sin(2.0 * kPi * i / 60.0);
        samples[i] =
            std::min(32767, std::max(-32768, samples[i] + beep_sample));
      }
    }
    wav_writer.WriteSamples(samples.data(), kAudioFrameLen);
  }

  return 0;
}

}  // namespace
}  // namespace test
}  // namespace webrtc

int main(int argc, char* argv[]) {
  return webrtc::test::main(argc, argv);
}
