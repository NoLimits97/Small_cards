#include "test_framework.hpp"

#include "mk/org_details.hpp"
#include "mk/repositories.hpp"

#include <QSqlQuery>
#include <QTemporaryDir>

#include <cstdint>
#include <string>

TEST(org_details_create_update_and_read) {
    QTemporaryDir directory;
    REQUIRE(directory.isValid());

    mk::Database database;
    database.create_new(directory.filePath("details.sqlite"));

    const int64_t id = mk::create_org_with_details(
        database,
        {0, "Организация"},
        {0, "1234567890", "123456789"});

    const auto created = mk::read_org_details(database, id);
    REQUIRE(created.has_value());
    REQUIRE_EQUAL(created->org_id, id);
    REQUIRE_EQUAL(created->inn, std::string("1234567890"));
    REQUIRE_EQUAL(created->kpp, std::string("123456789"));

    mk::update_org_with_details(
        database,
        {id, "Новое название"},
        {id, "0987654321", "987654321"});

    REQUIRE_EQUAL(
        mk::read_org(database, id)->name,
        std::string("Новое название"));

    const auto updated = mk::read_org_details(database, id);
    REQUIRE(updated.has_value());
    REQUIRE_EQUAL(updated->inn, std::string("0987654321"));
    REQUIRE_EQUAL(updated->kpp, std::string("987654321"));

    mk::delete_org(database, id);
    REQUIRE(!mk::read_org_details(database, id).has_value());
}

TEST(org_details_migration_and_repeated_open) {
    QTemporaryDir directory;
    REQUIRE(directory.isValid());
    const QString path = directory.filePath("legacy.sqlite");

    mk::Database database;
    database.create_new(path);
    mk::update_org(database, {1, "Название из M6"});
    database.close();

    database.open(path);
    REQUIRE_EQUAL(
        mk::read_org(database, 1)->name,
        std::string("Название из M6"));
    REQUIRE(!mk::read_org_details(database, 1).has_value());

    mk::update_org_with_details(
        database,
        {1, "После миграции"},
        {1, "1234567890", "123456789"});
    database.close();

    database.open(path);
    REQUIRE_EQUAL(
        mk::read_org(database, 1)->name,
        std::string("После миграции"));
    REQUIRE_EQUAL(
        mk::read_org_details(database, 1)->inn,
        std::string("1234567890"));
    REQUIRE_EQUAL(
        mk::read_org_details(database, 1)->kpp,
        std::string("123456789"));

    database.close();
    database.open(path);
    REQUIRE_EQUAL(
        mk::read_org_details(database, 1)->inn,
        std::string("1234567890"));
}

TEST(org_details_update_is_atomic) {
    QTemporaryDir directory;
    REQUIRE(directory.isValid());

    mk::Database database;
    database.create_new(directory.filePath("atomic.sqlite"));
    const int64_t id = mk::create_org_with_details(
        database, {0, "До"}, {0, "111", "222"});

    QSqlQuery trigger(database.sql());
    REQUIRE(trigger.exec(
        "CREATE TRIGGER reject_details BEFORE UPDATE ON OrgDetails "
        "BEGIN SELECT RAISE(ABORT, 'details rejected'); END"));

    REQUIRE_THROWS(mk::update_org_with_details(
        database, {id, "После"}, {id, "333", "444"}));

    REQUIRE_EQUAL(mk::read_org(database, id)->name, std::string("До"));
    REQUIRE_EQUAL(
        mk::read_org_details(database, id)->inn,
        std::string("111"));
    REQUIRE_EQUAL(
        mk::read_org_details(database, id)->kpp,
        std::string("222"));
}

TEST(org_details_create_is_atomic) {
    QTemporaryDir directory;
    REQUIRE(directory.isValid());

    mk::Database database;
    database.create_new(directory.filePath("create_atomic.sqlite"));

    REQUIRE(!mk::read_org_details(database, 1).has_value());

    QSqlQuery trigger(database.sql());
    REQUIRE(trigger.exec(
        "CREATE TRIGGER reject_new_details BEFORE INSERT ON OrgDetails "
        "BEGIN SELECT RAISE(ABORT, 'details rejected'); END"));

    REQUIRE_THROWS(mk::create_org_with_details(
        database, {0, "Не сохранена"}, {0, "111", "222"}));

    QSqlQuery query(database.sql());
    REQUIRE(query.exec(
        "SELECT COUNT(*) FROM Org WHERE name='Не сохранена'"));
    REQUIRE(query.next());
    REQUIRE_EQUAL(query.value(0).toInt(), 0);
}
