#ifndef MK_DATABASE_HPP
#define MK_DATABASE_HPP

#include <QSqlDatabase>

#include <QString>

namespace mk {

class Database {
public:
    Database();
    ~Database();

    Database(const Database&) = delete;
    Database& operator=(const Database&) = delete;

    Database(Database&& other);
    Database& operator=(Database&& other);

    void open(const QString& path);
    void create_new(const QString& path);

    void execute_schema();
    void execute_seed();

    void close();

    void transaction();
    void commit();
    void rollback();

    QSqlDatabase& sql();
    const QSqlDatabase& sql() const;

private:
    QSqlDatabase database_;
    QString connection_name_;
};

}

#endif
