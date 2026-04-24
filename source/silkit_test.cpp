#include "silkit/SilKit.hpp"
#include "silkit/services/can/all.hpp"

#include <iostream>
#include <thread>
#include <chrono>

using namespace SilKit;
using namespace SilKit::Services::Can;

int main()
{
    auto config = Config::ParticipantConfigurationFromString("{}");

    auto participant =
        SilKit::CreateParticipant(config, "SenderParticipant", "silkit://localhost:8500");

    auto* canController = participant->CreateCanController("CAN1", "CAN1");

    canController->AddFrameHandler(
        [](ICanController*, const CanFrameEvent& event) {
            std::cout << "[SILKIT] Received frame ID=" << event.frame.canId
                      << std::endl;
        });

    canController->Start();

    while (true)
    {
        std::vector<uint8_t> data = {0xAA, 0xBB, 0xCC};

        CanFrame frame;
        frame.canId = 0x456;
        frame.dlc = static_cast<uint8_t>(data.size());
        frame.dataField = data;

        canController->SendFrame(frame);

        std::cout << "[SILKIT] Sent frame ID=0x456" << std::endl;

        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    return 0;
}
