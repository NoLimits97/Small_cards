#include "mk/org_details.hpp"

#include "mk/repositories.hpp"

#include <QSqlError>
#include <QSqlQuery>
#include <QString>

#include <stdexcept>

namespace mk {
namespace {

void check(QSqlQuery& query) {
    if (!query.exec()) {
        throw std::runtime_error(query.lastError().text().toStdString());
    }
}

void ensure_table(Database& database) {
    QSqlQuery query(database.sql());
    query.prepare(
        "CREATE TABLE IF NOT EXISTS OrgDetails ("
        "org_id INTEGER PRIMARY KEY REFERENCES Org(id) ON DELETE CASCADE,"
        "inn TEXT NOT NULL DEFAULT '',"
        "kpp TEXT NOT NULL DEFAULT '')");
    check(query);
}

void save_details(Database& database, const OrgDetails& details) {
    QSqlQuery query(database.sql());
    query.prepare(
        "INSERT INTO OrgDetails(org_id,inn,kpp) VALUES(?,?,?) "
        "ON CONFLICT(org_id) DO UPDATE SET "
        "inn=excluded.inn,kpp=excluded.kpp");
    query.addBindValue(details.org_id);
    query.addBindValue(QString::fromStdString(details.inn));
    query.addBindValue(QString::fromStdString(details.kpp));
    check(query);
}

}

std::optional<OrgDetails> read_org_details(
    Database& database,
    int64_t org_id) {
    ensure_table(database);

    QSqlQuery query(database.sql());
    query.prepare(
        "SELECT org_id,inn,kpp FROM OrgDetails WHERE org_id=?");
    query.addBindValue(org_id);
    check(query);

    if (!query.next()) {
        return std::nullopt;
    }

    return OrgDetails{
        query.value(0).toLongLong(),
        query.value(1).toString().toStdString(),
        query.value(2).toString().toStdString()
    };
}

int64_t create_org_with_details(
    Database& database,
    const Org& organization,
    const OrgDetails& details) {
    ensure_table(database);
    database.transaction();

    try {
        const int64_t org_id = create_org(database, organization);
        OrgDetails saved = details;
        saved.org_id = org_id;
        save_details(database, saved);
        database.commit();
        return org_id;
    } catch (...) {
        database.rollback();
        throw;
    }
}

void update_org_with_details(
    Database& database,
    const Org& organization,
    const OrgDetails& details) {
    if (organization.id != details.org_id) {
        throw std::invalid_argument(
            "organization and details id mismatch");
    }

    ensure_table(database);
    database.transaction();

    try {
        update_org(database, organization);
        save_details(database, details);
        database.commit();
    } catch (...) {
        database.rollback();
        throw;
    }
}

}
