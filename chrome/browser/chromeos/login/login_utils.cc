// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/login_utils.h"

#include "base/command_line.h"
#include "base/file_path.h"
#include "base/lock.h"
#include "base/nss_util.h"
#include "base/path_service.h"
#include "base/scoped_ptr.h"
#include "base/singleton.h"
#include "chrome/browser/browser_init.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/cros/login_library.h"
#include "chrome/browser/chromeos/external_cookie_handler.h"
#include "chrome/browser/chromeos/login/cookie_fetcher.h"
#include "chrome/browser/chromeos/login/google_authenticator.h"
#include "chrome/browser/chromeos/login/pam_google_authenticator.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/browser/net/url_request_context_getter.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/profile_manager.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/notification_observer.h"
#include "chrome/common/notification_registrar.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/notification_type.h"
#include "googleurl/src/gurl.h"
#include "net/base/cookie_store.h"
#include "net/url_request/url_request_context.h"
#include "views/widget/widget_gtk.h"

namespace chromeos {

class LoginUtilsImpl : public LoginUtils,
                       public NotificationObserver {
 public:
  LoginUtilsImpl() {
    registrar_.Add(
        this,
        NotificationType::LOGIN_USER_CHANGED,
        NotificationService::AllSources());
  }

  // Invoked after the user has successfully logged in. This launches a browser
  // and does other bookkeeping after logging in.
  virtual void CompleteLogin(const std::string& username,
                             const std::string& credentials);

  // Creates and returns the authenticator to use. The caller owns the returned
  // Authenticator and must delete it when done.
  virtual Authenticator* CreateAuthenticator(LoginStatusConsumer* consumer);

  // NotificationObserver implementation.
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

 private:
  NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(LoginUtilsImpl);
};

class LoginUtilsWrapper {
 public:
  LoginUtilsWrapper() {}

  LoginUtils* get() {
    AutoLock create(create_lock_);
    if (!ptr_.get())
      reset(new LoginUtilsImpl);
    return ptr_.get();
  }

  void reset(LoginUtils* ptr) {
    ptr_.reset(ptr);
  }

 private:
  Lock create_lock_;
  scoped_ptr<LoginUtils> ptr_;

  DISALLOW_COPY_AND_ASSIGN(LoginUtilsWrapper);
};

void LoginUtilsImpl::CompleteLogin(const std::string& username,
                                   const std::string& credentials) {
  LOG(INFO) << "Completing login for " << username;

  if (CrosLibrary::Get()->EnsureLoaded())
    CrosLibrary::Get()->GetLoginLibrary()->StartSession(username, "");

  UserManager::Get()->UserLoggedIn(username);

  // Now launch the initial browser window.
  const CommandLine& command_line = *CommandLine::ForCurrentProcess();
  FilePath user_data_dir;
  PathService::Get(chrome::DIR_USER_DATA, &user_data_dir);
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  // The default profile will have been changed because the ProfileManager
  // will process the notification that the UserManager sends out.
  Profile* profile = profile_manager->GetDefaultProfile(user_data_dir);

  if (!CommandLine::ForCurrentProcess()->HasSwitch(switches::kInChromeAuth)) {
    ExternalCookieHandler::GetCookies(command_line, profile);
    DoBrowserLaunch(profile);
  } else {
    // Take the credentials passed in and try to exchange them for
    // full-fledged Google authentication cookies.  This is
    // best-effort; it's possible that we'll fail due to network
    // troubles or some such.  Either way, |cf| will call
    // DoBrowserLaunch on the UI thread when it's done, and then
    // delete itself.
    CookieFetcher* cf = new CookieFetcher(profile);
    cf->AttemptFetch(credentials);
  }
}

Authenticator* LoginUtilsImpl::CreateAuthenticator(
    LoginStatusConsumer* consumer) {
  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kInChromeAuth))
    return new GoogleAuthenticator(consumer);
  return new PamGoogleAuthenticator(consumer);
}

void LoginUtilsImpl::Observe(NotificationType type,
                             const NotificationSource& source,
                             const NotificationDetails& details) {
  if (type == NotificationType::LOGIN_USER_CHANGED)
    base::OpenPersistentNSSDB();
}

LoginUtils* LoginUtils::Get() {
  return Singleton<LoginUtilsWrapper>::get()->get();
}

void LoginUtils::Set(LoginUtils* mock) {
  Singleton<LoginUtilsWrapper>::get()->reset(mock);
}

void LoginUtils::DoBrowserLaunch(Profile* profile) {
  std::vector<UserManager::User> users = UserManager::Get()->GetUsers();
  if (!users.empty())
    UserManager::Get()->DownloadUserImage(users[0].email());
  LOG(INFO) << "Launching browser...";
  BrowserInit browser_init;
  int return_code;
  browser_init.LaunchBrowser(*CommandLine::ForCurrentProcess(),
                             profile,
                             std::wstring(),
                             true,
                             &return_code);
}

}  // namespace chromeos
