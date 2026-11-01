#ifndef MK_ORG_DETAILS_HPP
#define MK_ORG_DETAILS_HPP

#include "mk/database.hpp"
#include "mk/model.hpp"

#include <cstdint>
#include <optional>
#include <string>

namespace mk {

struct OrgDetails {
    int64_t org_id = 0;
    std::string inn;
    std::string kpp;
};

std::optional<OrgDetails> read_org_details(
    Database& database,
    int64_t org_id);

int64_t create_org_with_details(
    Database& database,
    const Org& organization,
    const OrgDetails& details);

void update_org_with_details(
    Database& database,
    const Org& organization,
    const OrgDetails& details);

}

#endif
