#include "mk/database.hpp"

#include <QFile>
#include <QResource>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

#include <stdexcept>
#include <utility>

void initialize_storage_resources() {
    static const bool initialized = [] {
        Q_INIT_RESOURCE(storage_resources);
        return true;
    }();

    (void)initialized;
}

namespace mk {
namespace {

void throw_sql(const QSqlQuery& query) {
    throw std::runtime_error(query.lastError().text().toStdString());
}

QString read_resource(const QString& path) {
    initialize_storage_resources();

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        throw std::runtime_error(
            ("cannot open resource: " + path).toStdString());
    }

    return QString::fromUtf8(file.readAll());
}

void execute_script(QSqlDatabase& database, const QString& script) {
    const QStringList statements = script.split(';', Qt::SkipEmptyParts);

    for (const QString& statement : statements) {
        const QString sql = statement.trimmed();
        if (sql.isEmpty()) {
            continue;
        }

        QSqlQuery query(database);
        if (!query.exec(sql)) {
            throw_sql(query);
        }
    }
}

}

Database::Database() = default;

Database::~Database() {
    close();
}

Database::Database(Database&& other)
    : database_(other.database_),
      connection_name_(std::move(other.connection_name_)) {
    other.database_ = QSqlDatabase();
    other.connection_name_.clear();
}

Database& Database::operator=(Database&& other) {
    if (this == &other) {
        return *this;
    }

    close();

    database_ = other.database_;
    connection_name_ = std::move(other.connection_name_);

    other.database_ = QSqlDatabase();
    other.connection_name_.clear();

    return *this;
}

void Database::open(const QString& path) {
    close();

    connection_name_ =
        QString("mk_storage_%1").arg(
            reinterpret_cast<quintptr>(this));

    database_ = QSqlDatabase::addDatabase("QSQLITE", connection_name_);
    database_.setDatabaseName(path);

    if (!database_.open()) {
        throw std::runtime_error(
            database_.lastError().text().toStdString());
    }

    QSqlQuery query(database_);
    if (!query.exec("PRAGMA foreign_keys = ON")) {
        throw_sql(query);
    }
}

void Database::create_new(const QString& path) {
    open(path);

    QSqlQuery query(database_);
    if (!query.exec("PRAGMA journal_mode = WAL")) {
        throw_sql(query);
    }

    execute_schema();
    execute_seed();
}

void Database::execute_schema() {
    execute_script(
        database_,
        read_resource(":/mk/db/schema.sql"));
}

void Database::execute_seed() {
    execute_script(
        database_,
        read_resource(":/mk/db/seed.sql"));
}

void Database::close() {
    if (!connection_name_.isEmpty()) {
        if (database_.isOpen()) {
            database_.close();
        }

        const QString name = connection_name_;
        database_ = QSqlDatabase();
        QSqlDatabase::removeDatabase(name);
        connection_name_.clear();
    }
}

void Database::transaction() {
    if (!database_.transaction()) {
        throw std::runtime_error(
            database_.lastError().text().toStdString());
    }
}

void Database::commit() {
    if (!database_.commit()) {
        throw std::runtime_error(
            database_.lastError().text().toStdString());
    }
}

void Database::rollback() {
    if (!database_.rollback()) {
        throw std::runtime_error(
            database_.lastError().text().toStdString());
    }
}

QSqlDatabase& Database::sql() {
    return database_;
}

const QSqlDatabase& Database::sql() const {
    return database_;
}

}
