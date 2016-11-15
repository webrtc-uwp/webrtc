#include "video_common_winrt.h"

Windows::UI::Core::CoreDispatcher^ webrtc::VideoCommonWinRT::windowDispatcher;

void webrtc::VideoCommonWinRT::SetCoreDispatcher(Windows::UI::Core::CoreDispatcher ^ inWindowDispatcher)
{
	windowDispatcher = inWindowDispatcher;
}

Windows::UI::Core::CoreDispatcher ^ webrtc::VideoCommonWinRT::GetCoreDispatcher()
{
	return windowDispatcher;
}
