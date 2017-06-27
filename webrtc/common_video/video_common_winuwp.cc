#include "video_common_winuwp.h"

Windows::UI::Core::CoreDispatcher^ webrtc::VideoCommonWinUWP::windowDispatcher;

void webrtc::VideoCommonWinUWP::SetCoreDispatcher(Windows::UI::Core::CoreDispatcher ^ inWindowDispatcher)
{
	windowDispatcher = inWindowDispatcher;
}

Windows::UI::Core::CoreDispatcher ^ webrtc::VideoCommonWinUWP::GetCoreDispatcher()
{
	return windowDispatcher;
}
