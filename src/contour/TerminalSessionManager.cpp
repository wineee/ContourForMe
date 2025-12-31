// SPDX-License-Identifier: Apache-2.0
#include <contour/ContourGuiApp.h>
#include <contour/TerminalSession.h>
#include <contour/TerminalSessionManager.h>

#include <vtbackend/primitives.h>

#include <vtpty/Process.h>
#if defined(VTPTY_LIBSSH2)
    #include <vtpty/SshSession.h>
#endif

#include <QtQml/QQmlEngine>
#include <QtCore/QString>

#include <algorithm>
#include <filesystem>
#include <string>

using namespace std::string_literals;

using std::make_unique;
using std::nullopt;

namespace contour
{

TerminalSessionManager::TerminalSessionManager(ContourGuiApp& app): _app { app }, _earlyExitThreshold {}
{
}

std::unique_ptr<vtpty::Pty> TerminalSessionManager::createPty(std::optional<std::string> cwd)
{
    auto const& profile = _app.config().profile(_app.profileName());
#if defined(VTPTY_LIBSSH2)
    if (!profile->ssh.value().hostname.empty())
        return make_unique<vtpty::SshSession>(
            profile->ssh.value(),
            std::bind(&TerminalSessionManager::requestSshHostkeyVerification,
                      this,
                      std::placeholders::_1,
                      std::placeholders::_2));
#endif
    if (cwd)
        profile->shell.value().workingDirectory = std::filesystem::path(cwd.value());
    return make_unique<vtpty::Process>(profile->shell.value(),
                                       vtpty::createPty(profile->terminalSize.value(), nullopt),
                                       profile->escapeSandbox.value());
}

void TerminalSessionManager::requestSshHostkeyVerification(
    vtpty::SshHostkeyVerificationRequest const& request,
    vtpty::SshHostkeyVerificationResponseCallback const& response)
{
    (void) request;

    // TODO: implement SSH host key verification dialog
    response(true);
}

TerminalSession* TerminalSessionManager::createSessionInBackground()
{
    // TODO: Remove dependency on app-knowledge and pass shell / terminal-size instead.
    // The GuiApp *or* (Global)Config could be made a global to be accessible from within QML.

    if (!_activeDisplay)
    {
        managerLog()("No active display found. something went wrong.");
    }

    if (!_allowCreation)
    {
        managerLog()("Session creation is disabled.");
        // try to find for the selected display a session to use

        for (auto& session: _sessions)
        {
            if (_displayStates[_activeDisplay].currentSession == session)
            {
                managerLog()("Found suitable session Returning it.");
                return session;
            }
        }
    }

#if !defined(_WIN32)
    auto ptyPath = [this]() -> std::optional<std::string> {
        if (_sessions.empty())
            return std::nullopt;
        auto& terminal = _sessions[0]->terminal();
        if (auto const* ptyProcess = dynamic_cast<vtpty::Process const*>(&terminal.device()))
            return ptyProcess->workingDirectory();
        return std::nullopt;
    }();
#else
    std::optional<std::string> ptyPath = std::nullopt;
    if (!_sessions.empty())
    {
        auto& terminal = _sessions[0]->terminal();
        {
            auto _l = std::scoped_lock { terminal };
            ptyPath = terminal.currentWorkingDirectory();
        }
    }
#endif

    auto* session = new TerminalSession(this, createPty(ptyPath), _app);
    managerLog()("Create new session with ID {}({}) at index {}",
                 session->id(),
                 (void*) session,
                 _sessions.size());

    auto const row = static_cast<int>(_sessions.size());
    beginInsertRows(QModelIndex(), row, row);
    _sessions.insert(_sessions.end(), session);
    endInsertRows();

    connect(session, &TerminalSession::sessionClosed, [this, session]() { removeSession(*session); });
    connect(session, &TerminalSession::titleChanged, this, [this, session]() {
        emitSessionDataChanged(session, { Roles::TitleRole });
        updateStatusLine();
    });

    // Claim ownership of this object, so that it will be deleted automatically by the QML's GC.
    //
    // QQmlEngine falsely assumed that the object would not be needed anymore at random times in active
    // sessions. This will work around it, by explicitly claiming ownership of the object.
    QQmlEngine::setObjectOwnership(session, QQmlEngine::CppOwnership);

    _allowCreation = false;
    return session;
}

void TerminalSessionManager::setSession(size_t index)
{
    Require(index <= _sessions.size());
    managerLog()(std::format("SET SESSION: index: {}, _sessions.size(): {}", index, _sessions.size()));

    if (index < _sessions.size())
        activateSession(_sessions[index]);
    else
        activateSession(createSessionInBackground());
}

TerminalSession* TerminalSessionManager::activateSession(TerminalSession* session, bool isNewSession)
{
    if (!session)
        return nullptr;

    // debug for displayStates
    for (auto& [display, state]: _displayStates)
    {
        managerLog()("display: {}, session: {}\n", (void*) display, (void*) state.currentSession);
    }

    managerLog()("Activating session ID {} {} at index {}",
                 session->id(),
                 (void*) session,
                 getSessionIndexOf(session).value_or(-1));

    // iterate over _displayStates to see if this session is already active
    for (auto& [display, state]: _displayStates)
    {
        if (display && state.currentSession == session)
        {
            if (!display->hasSession())
            {
                managerLog()("Display does not have a session will set it to another session.");
                continue;
            }
            managerLog()("Session is already active : (display {}, ID {} {})",
                         (void*) display,
                         session->id(),
                         (void*) session);
            return session;
        }
    }

    if (!_activeDisplay)
    {
        managerLog()("No active display found. something went wrong.");
    }

    if (!_allowSwitchOfTheSession)
    {
        _displayStates[nullptr].currentSession = session;
        _allowSwitchOfTheSession = true;
        return session;
    }

    auto& displayState = _displayStates[_activeDisplay];
    auto const previousIndex = getSessionIndexOf(displayState.currentSession);

    displayState.previousSession = displayState.currentSession;
    displayState.currentSession = session;
    updateStatusLine();

    if (previousIndex && *previousIndex < _sessions.size())
        emitSessionDataChanged(_sessions.at(*previousIndex), { Roles::ActiveRole });

    if (auto const nextIndex = getSessionIndexOf(session))
        emitSessionDataChanged(_sessions.at(*nextIndex), { Roles::ActiveRole });

    if (_activeDisplay)
    {

        auto const pixels = _activeDisplay->pixelSize();
        auto const totalPageSize = _activeDisplay->calculatePageSize();

        // Ensure that the existing session is resized to the display's size.
        if (!isNewSession)
        {
            managerLog()("Resize existing session to display size: {}x{}.",
                         _activeDisplay->width(),
                         _activeDisplay->height());
            auto const _ = std::scoped_lock { displayState.currentSession->terminal() };
            displayState.currentSession->terminal().resizeScreen(totalPageSize, pixels);
        }

        managerLog()(
            "Set display {} to session: {}({}).", (void*) _activeDisplay, session->id(), (void*) session);
        // resize terminal session before display is attached to it
        _activeDisplay->setSession(displayState.currentSession);

        // Resize active session after display is attached to it
        // to return a lost line
        {
            auto const _ = std::scoped_lock { displayState.currentSession->terminal() };
            displayState.currentSession->terminal().resizeScreen(totalPageSize, pixels);
        }
    }

    return session;
}

void TerminalSessionManager::FocusOnDisplay(display::TerminalDisplay* display)
{
    managerLog()("Setting active display to {}", (void*) display);
    _activeDisplay = display;

    // if we have a session in nullptr display, set it to this one
    if (_displayStates[nullptr].currentSession != nullptr)
    {
        _displayStates[_activeDisplay] = _displayStates[nullptr];
        _displayStates[nullptr].currentSession = nullptr;
    }

    // if this is new display, find a session to attach to
    if (_displayStates[_activeDisplay].currentSession == nullptr)
    {
        tryFindSessionForDisplayOrClose();
        return;
    }

    updateStatusLine();
    activateSession(_displayStates[_activeDisplay].currentSession);
}

TerminalSession* TerminalSessionManager::createSession()
{
    return activateSession(createSessionInBackground(), true /*force resize on before display-attach*/);
}

TerminalSession* TerminalSessionManager::addSession()
{
    allowCreation();
    return createSession();
}

void TerminalSessionManager::switchToPreviousTab()
{
    managerLog()("switch to previous tab (current: {}, previous: {})",
                 getSessionIndexOf(_displayStates[_activeDisplay].currentSession).value_or(-1),
                 getSessionIndexOf(_displayStates[_activeDisplay].previousSession).value_or(-1));

    activateSession(_displayStates[_activeDisplay].previousSession);
}

void TerminalSessionManager::switchToTabLeft()
{
    const auto currentSessionIndex = getCurrentSessionIndex();
    managerLog()(std::format("previous tab: currentSessionIndex: {}, _sessions.size(): {}",
                             currentSessionIndex,
                             _sessions.size()));

    if (currentSessionIndex > 0)
    {
        setSession(currentSessionIndex - 1);
    }
    else // wrap
    {
        setSession(_sessions.size() - 1);
    }
}

void TerminalSessionManager::switchToTabRight()
{
    const auto currentSessionIndex = getCurrentSessionIndex();

    managerLog()(std::format(
        "NEXT TAB: currentSessionIndex: {}, _sessions.size(): {}", currentSessionIndex, _sessions.size()));
    if (std::cmp_less(currentSessionIndex, _sessions.size() - 1))
    {
        setSession(currentSessionIndex + 1);
    }
    else // wrap
    {
        setSession(0);
    }
}

void TerminalSessionManager::switchToTab(int position)
{
    managerLog()("switchToTab from index {} to {} (out of {})",
                 getSessionIndexOf(_displayStates[_activeDisplay].currentSession).value_or(-1),
                 position - 1,
                 _sessions.size());

    if (1 <= position && position <= static_cast<int>(_sessions.size()))
        activateSession(_sessions[position - 1]);
}

void TerminalSessionManager::switchToTabAt(int index)
{
    switchToTab(index + 1);
}

void TerminalSessionManager::closeWindow()
{
    if (!_activeDisplay)
    {
        managerLog()("No active display found. Cannot close window.");
        return;
    }
    if (auto* session = _displayStates[_activeDisplay].currentSession)
    {
        managerLog()("Removing display {} from _displayStates.", (void*) _activeDisplay);
        removeSession(*session);
        _activeDisplay = nullptr;
    }
    else
    {
        managerLog()("No session in active display. Cannot close window.");
    }
}

void TerminalSessionManager::closeTab()
{

    if (!_activeDisplay || !_displayStates[_activeDisplay].currentSession)
    {
        managerLog()("Failed to close tab: no active display or no session in active display.");
        return;
    }

    managerLog()("Close tab: current session ID {}, index {}",
                 getSessionIndexOf(_displayStates[_activeDisplay].currentSession).value_or(-1),
                 _displayStates[_activeDisplay].currentSession->id());

    removeSession(*_displayStates[_activeDisplay].currentSession);
}

void TerminalSessionManager::closeTabAt(int index)
{
    if (index < 0 || index >= static_cast<int>(_sessions.size()))
        return;

    auto* session = _sessions.at(static_cast<std::size_t>(index));
    removeSession(*session);
}

void TerminalSessionManager::moveTabTo(int position)
{
    if (position < 1 || position > static_cast<int>(_sessions.size()))
        return;

    auto const targetIndex = position - 1;

    auto const currentIndexOpt =
        _activeDisplay ? getSessionIndexOf(_displayStates[_activeDisplay].currentSession) : std::nullopt;
    if (!currentIndexOpt)
        return;

    auto const currentIndex = static_cast<int>(*currentIndexOpt);
    if (currentIndex == targetIndex)
        return;

    auto const destination = targetIndex > currentIndex ? targetIndex + 1 : targetIndex;
    beginMoveRows(QModelIndex(), currentIndex, currentIndex, QModelIndex(), destination);

    auto* session = _sessions.at(static_cast<std::size_t>(currentIndex));
    _sessions.erase(_sessions.begin() + currentIndex);
    _sessions.insert(_sessions.begin() + targetIndex, session);

    endMoveRows();
    updateStatusLine();
}

void TerminalSessionManager::moveTabToLeft(TerminalSession* session)
{
    auto const maybeIndex = getSessionIndexOf(session);
    if (!maybeIndex)
        return;

    auto const index = static_cast<int>(maybeIndex.value());

    if (index > 0)
    {
        beginMoveRows(QModelIndex(), index, index, QModelIndex(), index - 1);
        auto* s = _sessions.at(static_cast<std::size_t>(index));
        _sessions.erase(_sessions.begin() + index);
        _sessions.insert(_sessions.begin() + index - 1, s);
        endMoveRows();
        updateStatusLine();
    }
}

void TerminalSessionManager::moveTabToRight(TerminalSession* session)
{
    auto const maybeIndex = getSessionIndexOf(session);
    if (!maybeIndex)
        return;

    auto const index = static_cast<int>(maybeIndex.value());

    if (index + 1 < static_cast<int>(_sessions.size()))
    {
        beginMoveRows(QModelIndex(), index, index, QModelIndex(), index + 2);
        auto* s = _sessions.at(static_cast<std::size_t>(index));
        _sessions.erase(_sessions.begin() + index);
        _sessions.insert(_sessions.begin() + index + 1, s);
        endMoveRows();
        updateStatusLine();
    }
}

void TerminalSessionManager::currentSessionIsTerminated()
{
    managerLog()("got notified that session is terminated, number of existing sessions: _sessions.size(): {}",
                 _sessions.size());
}

void TerminalSessionManager::removeSession(TerminalSession& thatSession)
{
    managerLog()("remove session: session: {}, _sessions.size(): {}", (void*) &thatSession, _sessions.size());

    auto i = std::ranges::find(_sessions, &thatSession);
    if (i == _sessions.end())
    {
        managerLog()("Session not found in session list.");
        return;
    }
    auto const row = static_cast<int>(std::distance(_sessions.begin(), i));
    beginRemoveRows(QModelIndex(), row, row);
    _sessions.erase(i);
    endRemoveRows();

    for (auto& [display, state]: _displayStates)
    {
        if (state.currentSession == &thatSession)
            state.currentSession = nullptr;
        if (state.previousSession == &thatSession)
            state.previousSession = nullptr;
    }

    tryFindSessionForDisplayOrClose();
    updateStatusLine();
}

void TerminalSessionManager::tryFindSessionForDisplayOrClose()
{
    managerLog()("Trying to find session for display: {}", (void*) _activeDisplay);
    if (_sessions.empty())
    {
        updateStatusLine();
        if (_activeDisplay)
            _activeDisplay->closeDisplay();
        return;
    }

    if (_activeDisplay)
    {
        auto& state = _displayStates[_activeDisplay];
        if (state.currentSession
            && std::ranges::find(_sessions, state.currentSession) != _sessions.end())
        {
            updateStatusLine();
            activateSession(state.currentSession);
            return;
        }
    }

    for (auto& session: _sessions)
    {
        bool safeToSwitch { true };
        for (auto& [display, state]: _displayStates)
        {
            if (display && (state.currentSession == session))
            {
                safeToSwitch = false;
                break;
            }
        }

        if (safeToSwitch)
        {
            managerLog()("Switching to session: {}", (void*) session);
            activateSession(session);
            return;
        }
    }

    activateSession(_sessions.front());
}

void TerminalSessionManager::updateColorPreference(vtbackend::ColorPreference const& preference)
{
    for (auto& session: _sessions)
        session->updateColorPreference(preference);
}

// {{{ QAbstractListModel
QVariant TerminalSessionManager::data(const QModelIndex& index, int role) const
{
    if (index.row() >= static_cast<int>(_sessions.size()))
        return QVariant {};

    auto* session = _sessions.at(static_cast<size_t>(index.row()));

    switch (role)
    {
        case Qt::DisplayRole:
        case Roles::TitleRole: return sessionTitle(session, static_cast<std::size_t>(index.row()));
        case Roles::IdRole: return QVariant(session->id());
        case Roles::ActiveRole: return QVariant(isActiveSession(session));
        default: return QVariant {};
    }
}

int TerminalSessionManager::rowCount(const QModelIndex& parent) const
{
    crispy::ignore_unused(parent);

    return static_cast<int>(_sessions.size());
}
// }}}

QHash<int, QByteArray> TerminalSessionManager::roleNames() const
{
    return {
        { Roles::IdRole, "id" },
        { Roles::TitleRole, "title" },
        { Roles::ActiveRole, "active" },
    };
}

bool TerminalSessionManager::canCloseWindow() const noexcept
{
    auto const displayCount = std::count_if(
        _displayStates.begin(), _displayStates.end(), [](auto const& pair) { return pair.first != nullptr; });

    if (_sessions.size() >= static_cast<size_t>(displayCount))
    {
        managerLog()(
            "Cannot close window: there are {} sessions, and {} displays.", _sessions.size(), displayCount);
        // If there are more sessions than displays, we cannot close the window.
        return false;
    }

    return true;
}

QString TerminalSessionManager::sessionTitle(TerminalSession const* session, std::size_t index) const
{
    if (!session)
        return QString("Tab %1").arg(static_cast<int>(index + 1));

    if (auto const name = session->name())
        return QString::fromStdString(*name);

    auto const windowTitle = session->terminal().windowTitle();
    if (!windowTitle.empty())
        return QString::fromStdString(windowTitle);

    return QString("Tab %1").arg(static_cast<int>(index + 1));
}

void TerminalSessionManager::emitSessionDataChanged(TerminalSession* session, QList<int> const& roles)
{
    if (!session)
        return;

    if (auto const indexOpt = getSessionIndexOf(session))
    {
        auto const row = static_cast<int>(*indexOpt);
        auto const modelIndex = createIndex(row, 0);
        emit dataChanged(modelIndex, modelIndex, roles);
    }
}

} // namespace contour
