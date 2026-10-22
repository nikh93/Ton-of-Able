#pragma once
#include "LibraryManager.h"
class NamLibrary final : public LibraryManager { public: NamLibrary() : LibraryManager("nam", { ".nam" }) {} };
