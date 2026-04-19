#include "RecommendController.h"
#include <drogon/drogon.h>
#include <json/json.h>
#include <set>
#include <functional>

namespace calcite {
namespace api {
namespace v1 {

Json::Value RecommendController::createResponse(int code, const std::string &message, const Json::Value &data) {
  Json::Value response;
  response["code"]    = code;
  response["message"] = message;
  if (!data.isNull()) {
    response["data"] = data;
  } else {
    response["data"] = Json::Value(Json::objectValue);
  }
  return response;
}

void RecommendController::verifyTokenAndGetUserId(const HttpRequestPtr &req, std::function<void(bool, int64_t)> callback) {
  std::string token = req->getHeader("Authorization");
  if (!token.empty() && token.find("Bearer ") == 0) {
    token = token.substr(7);
  }
  if (token.empty()) {
    token = req->getParameter("token");
  }
  if (token.empty()) {
    callback(false, 0);
    return;
  }

  authService_.verifyToken(token, [callback](bool valid, int64_t userId, const std::string &)
      {
        callback(valid, userId);
      });
}

void RecommendController::fetchNewUserTags(int64_t userId, std::function<void(const std::vector<std::string>&)> callback) {
  auto tags = std::make_shared<std::set<std::string>>();
  auto dbClient = drogon::app().getDbClient("default");

  // Step 4: 热门标签（最终步骤）
  std::function<void()> step4 = [this, tags, callback]() {
    auto hotTags = esClient_.getHotTagsSync(3);
    for (const auto& t : hotTags) {
      tags->insert(t);
    }
    callback(std::vector<std::string>(tags->begin(), tags->end()));
  };

  // Step 3: 搜索历史 Top2
  std::function<void()> step3 = [this, userId, tags, step4, dbClient]() {
    dbClient->execSqlAsync(
      "SELECT query, COUNT(*) AS freq FROM search_history WHERE user_id = ? GROUP BY query ORDER BY freq DESC LIMIT 2",
      [tags, step4](const drogon::orm::Result& result) {
        for (const auto& row : result) {
          tags->insert(row["query"].as<std::string>());
        }
        step4();
      },
      [step4](const drogon::orm::DrogonDbException& e) {
        LOG_ERROR << "Search history query failed: " << e.base().what();
        step4();
      },
      userId
    );
  };

  // Step 2: Top2 最新标签
  std::function<void()> step2 = [this, userId, tags, step3, dbClient]() {
    dbClient->execSqlAsync(
      "SELECT t.name FROM note n JOIN note_tag nt ON n.id = nt.note_id JOIN tag t ON nt.tag_id = t.id WHERE n.user_id = ? ORDER BY n.created_at DESC LIMIT 10",
      [tags, step3](const drogon::orm::Result& result) {
        int added = 0;
        for (const auto& row : result) {
          auto [it, inserted] = tags->insert(row["name"].as<std::string>());
          if (inserted) {
            added++;
            if (added >= 2) break;
          }
        }
        step3();
      },
      [step3](const drogon::orm::DrogonDbException& e) {
        LOG_ERROR << "Latest tags query failed: " << e.base().what();
        step3();
      },
      userId
    );
  };

  // Step 1: Top3 用户笔记标签
  dbClient->execSqlAsync(
    "SELECT t.name, COUNT(*) as cnt FROM note_tag nt JOIN note n ON nt.note_id = n.id JOIN tag t ON nt.tag_id = t.id WHERE n.user_id = ? GROUP BY t.name ORDER BY cnt DESC LIMIT 3",
    [tags, step2](const drogon::orm::Result& result) {
      for (const auto& row : result) {
        tags->insert(row["name"].as<std::string>());
      }
      step2();
    },
    [step2](const drogon::orm::DrogonDbException& e) {
      LOG_ERROR << "Top3 tags query failed: " << e.base().what();
      step2();
    },
    userId
  );
}

void RecommendController::fetchOldUserTags(int64_t userId, std::function<void(const std::vector<std::string>&)> callback) {
  auto tags = std::make_shared<std::set<std::string>>();
  auto dbClient = drogon::app().getDbClient("default");

  // Step 3: 热门标签（最终步骤）
  std::function<void()> step3 = [this, tags, callback]() {
    auto hotTags = esClient_.getHotTagsSync(3);
    for (const auto& t : hotTags) {
      tags->insert(t);
    }
    callback(std::vector<std::string>(tags->begin(), tags->end()));
  };

  // Step 2: 搜索历史 Top2
  std::function<void()> step2 = [this, userId, tags, step3, dbClient]() {
    dbClient->execSqlAsync(
      "SELECT query, COUNT(*) AS freq FROM search_history WHERE user_id = ? GROUP BY query ORDER BY freq DESC LIMIT 2",
      [tags, step3](const drogon::orm::Result& result) {
        for (const auto& row : result) {
          tags->insert(row["query"].as<std::string>());
        }
        step3();
      },
      [step3](const drogon::orm::DrogonDbException& e) {
        LOG_ERROR << "Search history query failed: " << e.base().what();
        step3();
      },
      userId
    );
  };

  // Step 1: 兴趣模型 Top5 标签（基于用户行为统计）
  dbClient->execSqlAsync(
    "SELECT t.name, (uts.view_count * 1 + uts.like_count * 3 + uts.collect_count * 7) * EXP(-0.1 * COALESCE(DATEDIFF(NOW(), uts.last_action_time), 30)) as score "
    "FROM user_tag_stat uts JOIN tag t ON uts.tag_id = t.id WHERE uts.user_id = ? ORDER BY score DESC LIMIT 5",
    [tags, step2](const drogon::orm::Result& result) {
      for (const auto& row : result) {
        tags->insert(row["name"].as<std::string>());
      }
      step2();
    },
    [step2](const drogon::orm::DrogonDbException& e) {
      LOG_ERROR << "Behavior tags query failed: " << e.base().what();
      step2();
    },
    userId
  );
}

void RecommendController::recommendNotes(const HttpRequestPtr &req, std::function<void(const HttpResponsePtr &)> &&callback) {
  verifyTokenAndGetUserId(req, [this, req, callback](bool valid, int64_t userId) {
    if (!valid) {
      auto resp = HttpResponse::newHttpJsonResponse(createResponse(1, "Token无效或已过期"));
      callback(resp);
      return;
    }

    // 解析分页参数
    int page = 1;
    int pageSize = 10;
    try {
      std::string pageStr = req->getParameter("page");
      std::string pageSizeStr = req->getParameter("page_size");
      if (!pageStr.empty()) page = std::stoi(pageStr);
      if (!pageSizeStr.empty()) pageSize = std::stoi(pageSizeStr);
      if (page < 1) page = 1;
      if (pageSize < 1) pageSize = 10;
      if (pageSize > 50) pageSize = 50;
    } catch (...) {
      page = 1;
      pageSize = 10;
    }
    int from = (page - 1) * pageSize;

    // 判断新老用户：最近30天行为数 < 20 为新用户
    auto dbClient = drogon::app().getDbClient("default");
    dbClient->execSqlAsync(
      "SELECT COUNT(*) as cnt FROM user_action WHERE user_id = ? AND created_at >= DATE_SUB(NOW(), INTERVAL 30 DAY)",
      [this, userId, from, pageSize, callback](const drogon::orm::Result& result) {
        int actionCount = result[0]["cnt"].as<int>();
        bool isNewUser = actionCount < 20;

        // 根据用户类型获取推荐标签集合
        auto tagsCallback = [this, from, pageSize, callback](const std::vector<std::string>& tags) {
          // ES 查询推荐笔记（tags 为空时会查询所有公开笔记，作为兜底）
          auto esResults = esClient_.searchByTagsSync(tags, from, pageSize);

          // 如果按标签查询无结果且标签非空，兜底查询所有公开笔记
          if (esResults.empty() && !tags.empty()) {
            esResults = esClient_.searchByTagsSync({}, from, pageSize);
          }

          Json::Value data(Json::arrayValue);
          for (const auto& r : esResults) {
            Json::Value noteJson;
            noteJson["id"] = static_cast<Json::Int64>(r.noteId);
            noteJson["title"] = r.title;
            noteJson["summary"] = r.summary;
            noteJson["created_at"] = r.createdAt;
            noteJson["updated_at"] = r.updatedAt;
            data.append(noteJson);
          }

          auto resp = HttpResponse::newHttpJsonResponse(createResponse(0, "获取推荐成功", data));
          callback(resp);
        };

        if (isNewUser) {
          fetchNewUserTags(userId, tagsCallback);
        } else {
          fetchOldUserTags(userId, tagsCallback);
        }
      },
      [this, callback](const drogon::orm::DrogonDbException& e) {
        LOG_ERROR << "Failed to check user action count: " << e.base().what();
        auto resp = HttpResponse::newHttpJsonResponse(createResponse(1, "推荐服务暂不可用"));
        callback(resp);
      },
      userId
    );
  });
}

} // namespace v1
} // namespace api
} // namespace calcite
