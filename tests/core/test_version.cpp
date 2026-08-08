#include <cstring>

#include <gtest/gtest.h>

#include "hven/core/version.h"

TEST(Version, NonNullNonEmptyMatchesProjectVersion) {
    const char *v = hven::version();
    ASSERT_NE(v, nullptr);
    EXPECT_GT(std::strlen(v), 0u);
    EXPECT_STREQ(v, "0.1.0");
}
