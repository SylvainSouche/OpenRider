// The coordinate systems OpenRider ships (share/common/openrider/crs.txt),
// loaded once for the tests.
#pragma once

#include "reproj/crs_registry.h"

#include <atf-c++.hpp>

inline const reproj::CrsRegistry &Shipped()
{
    static const reproj::CrsRegistry registry = [] {
        reproj::CrsRegistry loaded;
        for (const std::string &error : loaded.LoadFile(REPROJ_CRS_FILE))
            ATF_FAIL(error);
        return loaded;
    }();
    return registry;
}

// The shipped system of `code`, required to exist.
inline reproj::Crs ShippedCrs(int code)
{
    const reproj::Crs crs = Shipped().Find(code);
    ATF_REQUIRE(crs.Valid());
    return crs;
}
