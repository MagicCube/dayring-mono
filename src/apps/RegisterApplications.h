#pragma once

namespace platform::runtime {

class Shell;

}

namespace apps {

[[nodiscard]] bool registerApplications(platform::runtime::Shell& shell);

}
