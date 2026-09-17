#include "RegisterApplications.h"

#include <memory>

#include "../platform/runtime/Shell.h"
#include "common/PlaceholderApplication.h"
#include "shell/ShellApplication.h"
#include "typography/TypographyApplication.h"

namespace apps {

using platform::runtime::Application;
using platform::runtime::Residency;

bool registerApplications(platform::runtime::Shell& shell) {
    return shell.registerApplication(
               "typography",
               []() -> std::unique_ptr<Application> { return std::make_unique<typography::TypographyApplication>(); },
               Residency::Transient) &&
           shell.registerApplication(
               "shell", []() -> std::unique_ptr<Application> { return std::make_unique<shell::ShellApplication>(); },
               Residency::Resident) &&
           shell.registerApplication(
               "calendar",
               []() -> std::unique_ptr<Application> {
                   return std::make_unique<common::PlaceholderApplication>("Calendar");
               },
               Residency::Transient) &&
           shell.registerApplication(
               "test",
               []() -> std::unique_ptr<Application> {
                   return std::make_unique<common::PlaceholderApplication>("Test");
               },
               Residency::Transient);
}

}  // namespace apps
