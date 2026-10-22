#pragma once
#include "LibraryManager.h"
class IRLibrary final : public LibraryManager { public: IRLibrary() : LibraryManager("ir", { ".wav" }) {} };
