// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "SettingsPaneContent.h"
#include "PaneArgs.h"
#include "SettingsPaneContent.g.cpp"

using namespace winrt::Windows::Foundation;
using namespace winrt::Windows::UI::Xaml;
using namespace winrt::Microsoft::Terminal::Settings::Model;

namespace winrt::TerminalApp::implementation
{
    SettingsPaneContent::SettingsPaneContent(CascadiaSettings settings)
    {
        _root = winrt::Windows::UI::Xaml::Controls::Grid{};
        _root.VerticalAlignment(VerticalAlignment::Stretch);
        _root.HorizontalAlignment(HorizontalAlignment::Stretch);

        _sui = winrt::Microsoft::Terminal::Settings::Editor::MainPage{ settings };

        auto res = Windows::UI::Xaml::Application::Current().Resources();
        auto bg = res.Lookup(winrt::box_value(L"UnfocusedBorderBrush"));
        _root.Background(bg.try_as<Media::Brush>());

        _box = winrt::Windows::UI::Xaml::Controls::TextBox{};
        _box.Margin({ 10, 10, 10, 10 });
        _box.AcceptsReturn(true);
        _box.TextWrapping(TextWrapping::Wrap);
        _root.Children().Append(_box);
    }

    winrt::Windows::UI::Xaml::FrameworkElement SettingsPaneContent::GetRoot()
    {
        return _sui;
    }
    winrt::Windows::Foundation::Size SettingsPaneContent::MinSize()
    {
        return { 1, 1 };
    }
    void SettingsPaneContent::Focus(winrt::Windows::UI::Xaml::FocusState reason)
    {
        if (reason != FocusState::Unfocused)
        {
            _sui.as<Controls::Page>().Focus(reason);
        }
    }
    void SettingsPaneContent::Close()
    {
        CloseRequested.raise(*this, nullptr);
    }

    NewTerminalArgs SettingsPaneContent::GetNewTerminalArgs(const bool /* asContent */) const
    {
        // TODO! hey, can we somehow replicate std::vector<ActionAndArgs> SettingsTab::BuildStartupActions?
        return nullptr;
    }
}