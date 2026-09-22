#include <format>

#include "ThirdParty/ImGui/imgui.h"
#include "Console.h"
#include "OverlayStat.h"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <iomanip>
#include <cassert>
#include <chrono>
#include <ctime>
#include <sstream>

namespace
{
	FString GetCurrentTimeString()
	{
		auto Now = std::chrono::system_clock::now();
		std::time_t NowTime = std::chrono::system_clock::to_time_t(Now);

		std::tm LocalTime{};
		localtime_s(&LocalTime, &NowTime);

		std::ostringstream Stream;
		Stream.imbue(std::locale(""));
		Stream << std::put_time(&LocalTime, "%H:%M:%S");

		return FString{ Stream.str() };
	}

	const char* LogCategoryToString(ELogCategory Category)
	{
		switch (Category)
		{
		case ELogCategory::Editor:  return "[Editor]";
		case ELogCategory::Core:    return "[Core]";
		case ELogCategory::Render:  return "[Render]";
		case ELogCategory::Physics: return "[Physics]";
		case ELogCategory::Etc:     return "[Etc]";
		default:                    return "[Unknown]";
		}
	}

	const char* LogLevelToString(ELogLevel Level)
	{
		switch (Level)
		{
		case ELogLevel::Log:     return "[Log]";
		case ELogLevel::Warning: return "[Warning]";
		case ELogLevel::Error:   return "[Error]";
		case ELogLevel::Fatal:   return "[Fatal]";
		default:                 return "[Unknown]";
		}
	}

	ImVec4 GetLogLevelColor(ELogLevel Level)
	{
		switch (Level)
		{
		case ELogLevel::Warning:
			return ImVec4(1.0f, 1.0f, 0.0f, 1.0f);
		case ELogLevel::Error:
			return ImVec4(1.0f, 0.0f, 0.0f, 1.0f);
		case ELogLevel::Fatal:
			return ImVec4(1.0f, 0.0f, 1.0f, 1.0f);
		default:
			return ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
		}
	}

    constexpr const char* Commands[] = {"help", "history", "clear", "echo", "PODO", "stat memory", "stat fps", "stat none"};

    std::string Lower(std::string Text)
    {
        for (char& C : Text) C = static_cast<char>(std::tolower(static_cast<unsigned char>(C)));
        return Text;
    }

    std::string FormatMessage(const FConsoleMessage& Message)
    {
        std::string Text = std::format("{} {} {} ", Message.Time.CStr(),
            LogLevelToString(Message.Level), LogCategoryToString(Message.Category));
        if (Message.SourceLine > 0)
        {
            const std::string_view Path(Message.SourceFile.CStr());
            const auto Slash = Path.find_last_of("/\\");
            Text += std::format("[{}:{}] ", Path.substr(Slash == std::string_view::npos ? 0 : Slash + 1), Message.SourceLine);
        }
        Text += Message.Text.CStr();
        return Text;
    }

    void LevelButton(const char* Label, bool& Visible)
    {
        if (ImGui::Selectable(Label, Visible, ImGuiSelectableFlags_DontClosePopups,
            ImVec2(ImGui::CalcTextSize(Label).x, 0))) Visible = !Visible;
    }

}

ConsoleWindow::ConsoleWindow()
	: mTitle("Console Window")
{
}

ConsoleWindow& ConsoleWindow::Get()
{
	static ConsoleWindow instance;
	return instance;
}

void ConsoleWindow::Init(std::string_view title, int capacity)
{
	mTitle = title;
	mCapacity = static_cast<size_t>((std::max)(1, capacity));

	Clear();

	mMessages.Reserve(mCapacity);

	mPendingBuffers[0].Reserve(64);
	mPendingBuffers[1].Reserve(64);

	ImGuiIO& io = ImGui::GetIO();
	mFont = io.Fonts->AddFontFromFileTTF("Assets/Fonts/consola.ttf", 16.0f);
}

void ConsoleWindow::Draw()
{
    FlushPending();
    if (mFont) ImGui::PushFont(mFont);
    ImGui::SetNextWindowSize(ImVec2(720, 240), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin(mTitle.CStr(), nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_MenuBar))
    {
        ImGui::End();
        if (mFont) ImGui::PopFont();
        return;
    }

    bool Copy = false;
    if (ImGui::BeginMenuBar())
    {
        if (ImGui::BeginMenu("Actions"))
        {
            Copy = ImGui::MenuItem("Copy visible logs");
            if (ImGui::MenuItem("Clear")) Clear();
            ImGui::MenuItem("Auto-scroll", nullptr, &mbAutoScroll);
            ImGui::EndMenu();
        }
        LevelButton("Log", mbShowLog);
        LevelButton("Warning", mbShowWarning);
        LevelButton("Error", mbShowError);
        LevelButton("Fatal", mbShowFatal);
        ImGui::EndMenuBar();
    }
    ImGui::SetNextItemWidth(-1);
    if (ImGui::InputTextWithHint("##Filter", "Filter (include,-exclude)", mFilter.InputBuf, IM_ARRAYSIZE(mFilter.InputBuf)))
        mFilter.Build();

    const float FooterHeight = ImGui::GetFrameHeightWithSpacing() * 2.0f;
    const bool DrawLogs = ImGui::BeginChild("ConsoleMessage", ImVec2(0, -FooterHeight),
        ImGuiChildFlags_Borders, ImGuiWindowFlags_HorizontalScrollbar);
    if (DrawLogs && ImGui::BeginPopupContextWindow())
    {
        if (ImGui::MenuItem("Copy visible logs")) Copy = true;
        if (ImGui::MenuItem("Clear")) Clear();
        ImGui::EndPopup();
    }
    const bool WasAtBottom = ImGui::GetScrollY() >= ImGui::GetScrollMaxY();
    std::string Clipboard;
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4, 1));
    if (DrawLogs || Copy)
    {
        for (size_t Index = 0; Index < mCount; ++Index)
        {
            const auto& Message = GetMessage(Index);
            const bool Visible = Message.Level == ELogLevel::Log ? mbShowLog :
                Message.Level == ELogLevel::Warning ? mbShowWarning :
                Message.Level == ELogLevel::Error ? mbShowError : mbShowFatal;
            if (!Visible) continue;
            const auto Text = FormatMessage(Message);
            if (!mFilter.PassFilter(Text.c_str())) continue;
            if (Copy) { Clipboard += Text; Clipboard += '\n'; }
            if (DrawLogs)
            {
                ImGui::PushStyleColor(ImGuiCol_Text, GetLogLevelColor(Message.Level));
                ImGui::TextUnformatted(Text.c_str());
                ImGui::PopStyleColor();
                if (Message.SourceLine > 0 && ImGui::IsItemHovered())
                    ImGui::SetTooltip("%s:%d", Message.SourceFile.CStr(), Message.SourceLine);
            }
        }
        if (Copy) ImGui::SetClipboardText(Clipboard.c_str());
    }
    if (DrawLogs)
    {
        if (mbScrollToBottom || (mbAutoScroll && WasAtBottom)) ImGui::SetScrollHereY(1.0f);
        mbScrollToBottom = false;
    }
    ImGui::PopStyleVar();
    ImGui::EndChild();
    ImGui::Separator();
    ImGui::SetNextItemWidth(-1);
    const auto Flags = ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_EscapeClearsAll
        | ImGuiInputTextFlags_CallbackCompletion | ImGuiInputTextFlags_CallbackHistory;
    if (ImGui::InputText("##ConsoleInput", mInputBuffer, sizeof(mInputBuffer), Flags, TextEditCallbackStub, this))
    {
        ExecuteCommand(mInputBuffer);
        mInputBuffer[0] = '\0';
        ImGui::SetKeyboardFocusHere(-1);
    }
    ImGui::SetItemDefaultFocus();
    ImGui::TextDisabled("help: commands | Tab: complete | Up/Down: history");
    ImGui::End();
    if (mFont) ImGui::PopFont();
}

void ConsoleWindow::AddLog(ELogLevel Level, ELogCategory Category, std::string_view Text, const char* File, int Line)
{
	FConsoleMessage Message;

	Message.Time = GetCurrentTimeString();
	Message.Level = Level;
	Message.Category = Category;
	Message.Text = Text;
	if (File) Message.SourceFile = std::string_view(File);
	Message.SourceLine = File ? Line : 0;

	{
		std::lock_guard<std::mutex> Lock( mPendingMutex );
		mPendingBuffers[mWriteBufferIndex].Add(Message);
	}
}

void ConsoleWindow::PushMessage(FConsoleMessage Message)
{
	if (mCount < mCapacity)
	{
		mMessages.Add(Message);
		++mCount;
		return;
	}

	mMessages[mFront] = Message;
	mFront = (mFront + 1) % mCapacity;
}

void ConsoleWindow::FlushPending()
{
	{
		std::lock_guard<std::mutex> Lock(mPendingMutex);
		std::swap(mWriteBufferIndex, mReadBufferIndex);
	}

	TArray<FConsoleMessage>& ReadBuffer = mPendingBuffers[mReadBufferIndex];
	for (const FConsoleMessage& Message : ReadBuffer)
	{
		PushMessage(Message);
	}

	ReadBuffer.Reset(0);
}

const FConsoleMessage& ConsoleWindow::GetMessage(size_t Index) const
{
	assert(Index < mCount);
	assert(mCount > 0);

	// mFront는 현재 가장 오래된 메시지 위치
	const uint32 CircularIndex = (mFront + Index) % mCount;

	return mMessages[CircularIndex];
}

void ConsoleWindow::Clear()
{
	{
		std::lock_guard<std::mutex> Lock(
			mPendingMutex
		);

		mPendingBuffers[0].Reset(0);
		mPendingBuffers[1].Reset(0);
	}

	mMessages.Reset(0);

	mFront = 0;
	mCount = 0;
}

void ConsoleWindow::ExecuteCommand( const char* Input)
{
    std::string Line(Input);
    const auto Start = Line.find_first_not_of(" \t\r\n");
    if (Start == std::string::npos) return;
    Line = Line.substr(Start, Line.find_last_not_of(" \t\r\n") - Start + 1);
    std::erase(mCommandHistory, Line);
    if (mCommandHistory.size() >= MaxCommandHistory) mCommandHistory.erase(mCommandHistory.begin());
    mCommandHistory.push_back(Line);
    mHistoryPosition = -1;
    mHistoryDraft.clear();
    mbScrollToBottom = true;
    AddLogFormat(ELogLevel::Log, ELogCategory::Editor, "# {}", Line);
    std::istringstream Stream(Line);


	std::string Command;
	Stream >> Command;
	Command = Lower(Command);

	if (Command == "clear")
	{
		Clear();
	}
	else if (Command == "help")
	{
		AddLog(
			ELogLevel::Log,
			ELogCategory::Etc,
			"Commands: help, history, clear, echo, PODO, stat memory, stat fps, stat none");
	}
    else if (Command == "history")
    {
        const size_t First = mCommandHistory.size() > 10 ? mCommandHistory.size() - 10 : 0;
        for (size_t Index = First; Index < mCommandHistory.size(); ++Index)
            AddLogFormat(ELogLevel::Log, ELogCategory::Editor, "{}: {}", Index, mCommandHistory[Index]);
    }
	else if (Command == "echo")
	{
		std::string Text;
		std::getline(Stream >> std::ws, Text);

		AddLog(
			ELogLevel::Log,
			ELogCategory::Core,
			Text);
	}
	else if (Command == "podo")
	{
		AddLog(
			ELogLevel::Fatal,
			ELogCategory::Etc,
			"***** PODO ENGINE *****\n"
			"\n"
			"******* WEEK  1 *******\n"
			"*** LKH LSE KDH KSH ***\n"
			"******* WEEK  2 *******\n"
			"*** KSH KHW CHS LJY ***\n"
			"******* WEEK  3 *******\n"
			"*** HHJ KHG CHS GJW ***\n"
			"***********************\n");

	}
	else if (Command == "stat")
	{
		std::string statname;
		Stream >> statname;
		statname = Lower(statname);

		if (statname == "memory")
		{
			OverlayStatWindow::GetInstance().ActivateMemoryStat();
		}
		else if (statname == "fps")
		{
			OverlayStatWindow::GetInstance().ActivateFpsStat();
		}
		else if (statname == "none")
		{
			OverlayStatWindow::GetInstance().DeactivateAllStat();
		}
        else AddLog(ELogLevel::Warning, ELogCategory::Editor, "Usage: stat memory | stat fps | stat none");
	}


	else
	{
		AddLog(
			ELogLevel::Warning,
			ELogCategory::Core,
			"Unknown command");
	}
}

int ConsoleWindow::TextEditCallbackStub(ImGuiInputTextCallbackData* Data)
{
    return static_cast<ConsoleWindow*>(Data->UserData)->TextEditCallback(Data);
}

int ConsoleWindow::TextEditCallback(ImGuiInputTextCallbackData* Data)
{
    if (Data->EventFlag == ImGuiInputTextFlags_CallbackHistory)
    {
        const int Previous = mHistoryPosition;
        if (Data->EventKey == ImGuiKey_UpArrow && !mCommandHistory.empty())
        {
            if (mHistoryPosition == -1)
            {
                mHistoryDraft.assign(Data->Buf, Data->BufTextLen);
                mHistoryPosition = static_cast<int>(mCommandHistory.size()) - 1;
            }
            else if (mHistoryPosition > 0) --mHistoryPosition;
        }
        else if (Data->EventKey == ImGuiKey_DownArrow && mHistoryPosition != -1)
        {
            if (++mHistoryPosition >= static_cast<int>(mCommandHistory.size())) mHistoryPosition = -1;
        }
        if (Previous != mHistoryPosition)
        {
            const auto& Text = mHistoryPosition == -1 ? mHistoryDraft : mCommandHistory[mHistoryPosition];
            Data->DeleteChars(0, Data->BufTextLen);
            Data->InsertChars(0, Text.c_str());
        }
    }
    else if (Data->EventFlag == ImGuiInputTextFlags_CallbackCompletion)
    {
        // Complete the entire command prefix, including the second word in "stat fps".
        int Start = 0;
        while (Start < Data->CursorPos && std::isspace(static_cast<unsigned char>(Data->Buf[Start]))) ++Start;
        const auto Prefix = Lower(std::string(Data->Buf + Start, Data->CursorPos - Start));
        std::vector<std::string> Matches;
        for (const char* Command : Commands)
            if (Lower(Command).starts_with(Prefix)) Matches.emplace_back(Command);
        if (Matches.empty())
            AddLogFormat(ELogLevel::Log, ELogCategory::Editor, "No match for '{}'", Prefix);
        else
        {
            std::string Completion = Matches.front();
            for (const auto& Match : Matches)
            {
                size_t Common = 0;
                while (Common < Completion.size() && Common < Match.size()
                    && std::tolower(static_cast<unsigned char>(Completion[Common])) ==
                       std::tolower(static_cast<unsigned char>(Match[Common]))) ++Common;
                Completion.resize(Common);
            }
            if (Matches.size() == 1 || Completion.size() > Prefix.size())
            {
                // Replace the rest of the word too if Tab is pressed in the middle of it.
                int End = Data->CursorPos;
                while (End < Data->BufTextLen && !std::isspace(static_cast<unsigned char>(Data->Buf[End]))) ++End;
                Data->DeleteChars(Start, End - Start);
                Data->InsertChars(Start, Completion.c_str());
                if (Matches.size() == 1 && Data->CursorPos == Data->BufTextLen)
                    Data->InsertChars(Data->CursorPos, " ");
            }
            if (Matches.size() > 1)
                for (const auto& Match : Matches)
                    AddLogFormat(ELogLevel::Log, ELogCategory::Editor, "  {}", Match);
        }
        mbScrollToBottom = true;
    }
    return 0;
}
