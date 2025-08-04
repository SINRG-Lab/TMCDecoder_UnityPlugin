#include "TVMDecoder.h"
#include "DecoderManager.h"
#include <iostream>

int main() {
    {
        auto testDecoder = DecoderManager::CreateDecoder(
            "test", "/Users/joshua/Documents/Co-Op_Mallesh/Test_Output", true);

        if (testDecoder->LoadSequence(
                "/Users/joshua/TVMDecoder/Assets/StreamingAssets/mesh_frame_reference.obj",
                "/Users/joshua/TVMDecoder/Assets/StreamingAssets/delta_trajectories_f64.bin",
                "/Users/joshua/TVMDecoder/Assets/StreamingAssets/B_matrix.txt",
                "/Users/joshua/TVMDecoder/Assets/StreamingAssets/T_matrix.txt")) {
            std::cout << "Success!\n";
        } else {
            std::cout << "Failure :(\n";
            return 1;
        }

        auto paths = testDecoder->DecodeObjs();
        for (const auto& path : paths) {
            std::cout << "Wrote frame to: " << path << "\n";
        }

        // Explicit cleanup (optional)
        DecoderManager::DestroyDecoder("test");
    }

    // Everything cleaned up before here — no static logger access on exit
    return 0;
}
