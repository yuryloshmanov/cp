#ifndef CP_DATABASE_HPP
#define CP_DATABASE_HPP


#include <set>
#include <mutex>
#include <string>
#include <vector>
#include <sqlite3.h>


#include "user.hpp"
#include "auth.hpp"


// Thread-safe, based on sqlite3
class Database {
    sqlite3 *db{};
    char *err_msg{};
    sqlite3_stmt *stmt{};
    std::mutex mutex{};

    // TODO: add noexcept
    // doesn't lock, must be locked outside
    auto prepareStatement(const char *sqlQuery) -> bool;

    // doesn't lock, must be locked outside
    template<class... Args>
    auto bindStatement(Args... args) -> bool;

    // doesn't lock
    static auto getFormattedDatetime(time_t rawTime) -> std::string;

    // explicitly locks
    auto executeSqlQuery(const std::string &sql) -> bool;

    // explicitly locks
    auto getUserPassword(const std::string &username) -> std::string;

    // implicitly locks by getUserId
    auto isUserExist(const std::string &username) -> bool;

    // explicitly locks
    auto getChatId(const std::string &chatName) -> int32_t;

    // implicitly locks by getChatId
    auto isChatExists(const std::string &chatName) -> bool;

    auto getUsername(const int id) -> std::string {
        std::string sql = "SELECT Username FROM Users WHERE Id = ?";
        if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
            throw std::runtime_error("sqlite3_prepare_v2 error");
        }
        if (sqlite3_bind_int(stmt, 1, id) != SQLITE_OK) {
            throw std::runtime_error("sqlite3_bind_text error");
        }
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            return std::string(reinterpret_cast<const char *>(sqlite3_column_text(stmt, 0)));
        } else {
            return {};
        }
    }

public:
    Database();

    explicit Database(const std::string &path);

    ~Database();

    // explicitly locks
    auto getUserId(const std::string &username) -> int32_t;

    // explicitly locks
    auto getAllUsers() -> std::set<User>;

    // implicitly locks by isUserExists and getUserPassword
    auto authenticateUser(const std::string &username, const std::string &password) -> AuthenticationStatus;

    // explicitly locks
    auto createUser(const std::string &username, const std::string &password) -> void;

    // explicitly and implicitly locks
    auto createChat(const std::string &chatName, const int32_t &adminId, const std::vector<int32_t> &userIds) -> bool;

    // explicitly locks
    auto getChatName(int chatId) -> std::string;

    // explicitly and implicitly locks
    auto getChatsByTime(int32_t userId, time_t rawTime) -> std::vector<std::string>;

    // explicitly and implicitly locks
    auto createMessage(const std::string &chatName, int32_t senderId, time_t rawTime, const std::string &data) -> bool;

    // explicitly and implicitly locks
    auto getAllMessagesFromChat(const std::string &chatName, int32_t userId) -> std::vector<std::string>;

    // explicitly and implicitly locks
    auto getUserAllowedRawTime(int32_t chatId, int32_t userId) -> time_t;

    // explicitly and implicitly locks
    auto inviteUserToChat(
            const std::string &chatName,
            int32_t invitorId,
            int32_t userId,
            bool allowHistorySharing = false
    ) -> void;
};


#endif //CP_DATABASE_HPP
