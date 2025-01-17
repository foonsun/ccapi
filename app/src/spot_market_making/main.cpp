#include "app/spot_market_making_event_handler.h"
#include "ccapi_cpp/ccapi_session.h"
namespace ccapi {
Logger* Logger::logger = nullptr;  // This line is needed.
} /* namespace ccapi */
using ::ccapi::AppLogger;
using ::ccapi::CcapiLogger;
using ::ccapi::CsvWriter;
using ::ccapi::Logger;
using ::ccapi::Request;
using ::ccapi::Session;
using ::ccapi::SessionConfigs;
using ::ccapi::SessionOptions;
using ::ccapi::SpotMarketMakingEventHandler;
using ::ccapi::Subscription;
using ::ccapi::UtilSystem;
using ::ccapi::UtilTime;
int main(int argc, char** argv) {
  AppLogger appLogger;
  CcapiLogger ccapiLogger(&appLogger);
  Logger::logger = &ccapiLogger;
  std::string saveCsvDirectory = UtilSystem::getEnvAsString("SAVE_CSV_DIRECTORY");
  std::string nowISO = UtilTime::getISOTimestamp(UtilTime::now());
  std::string exchange = UtilSystem::getEnvAsString("EXCHANGE");
  std::string instrument = UtilSystem::getEnvAsString("INSTRUMENT");
  std::string privateTradeCsvFilename(nowISO + "__private_trade__" + exchange + "__" + instrument + ".csv"),
      orderUpdateCsvFilename(nowISO + "__order_update__" + exchange + "__" + instrument + ".csv"),
      accountBalanceCsvFilename(nowISO + "__account_balance__" + exchange + "__" + instrument + ".csv");
  if (!saveCsvDirectory.empty()) {
    privateTradeCsvFilename = saveCsvDirectory + "/" + privateTradeCsvFilename;
    orderUpdateCsvFilename = saveCsvDirectory + "/" + orderUpdateCsvFilename;
    accountBalanceCsvFilename = saveCsvDirectory + "/" + accountBalanceCsvFilename;
  }
  CsvWriter privateTradeCsvWriter(privateTradeCsvFilename), orderUpdateCsvWriter(orderUpdateCsvFilename), accountBalanceCsvWriter(accountBalanceCsvFilename);
  privateTradeCsvWriter.writeRow({
      "TIME",
      "TRADE_ID",
      "LAST_EXECUTED_PRICE",
      "LAST_EXECUTED_SIZE",
      "SIDE",
      "IS_MAKER",
      "ORDER_ID",
      "CLIENT_ORDER_ID",
      "FEE_QUANTITY",
      "FEE_ASSET",
  });
  privateTradeCsvWriter.flush();
  orderUpdateCsvWriter.writeRow({
      "TIME",
      "ORDER_ID",
      "CLIENT_ORDER_ID",
      "SIDE",
      "LIMIT_PRICE",
      "QUANTITY",
      "REMAINING_QUANTITY",
      "CUMULATIVE_FILLED_QUANTITY",
      "CUMULATIVE_FILLED_PRICE_TIMES_QUANTITY",
      "STATUS",
  });
  orderUpdateCsvWriter.flush();
  accountBalanceCsvWriter.writeRow({
      "TIME",
      "BASE_AVAILABLE_BALANCE",
      "QUOTE_AVAILABLE_BALANCE",
      "BEST_BID_PRICE",
      "BEST_ASK_PRICE",
  });
  accountBalanceCsvWriter.flush();
  SpotMarketMakingEventHandler eventHandler(&appLogger, &privateTradeCsvWriter, &orderUpdateCsvWriter, &accountBalanceCsvWriter);
  eventHandler.exchange = exchange;
  eventHandler.instrument = instrument;
  eventHandler.baseAvailableBalanceProportion = UtilSystem::getEnvAsDouble("BASE_AVAILABLE_BALANCE_PROPORTION");
  eventHandler.quoteAvailableBalanceProportion = UtilSystem::getEnvAsDouble("QUOTE_AVAILABLE_BALANCE_PROPORTION");
  double a = UtilSystem::getEnvAsDouble("INVENTORY_BASE_QUOTE_RATIO_TARGET");
  eventHandler.inventoryBasePortionTarget = a / (a + 1);
  eventHandler.halfSpreadMinimum = UtilSystem::getEnvAsDouble("SPREAD_PROPORTION_MINIMUM") / 2;
  eventHandler.halfSpreadMaximum = UtilSystem::getEnvAsDouble("SPREAD_PROPORTION_MAXIMUM") / 2;
  eventHandler.orderQuantityProportion = UtilSystem::getEnvAsDouble("ORDER_QUANTITY_PROPORTION");
  eventHandler.orderRefreshIntervalSeconds = UtilSystem::getEnvAsInt("ORDER_REFRESH_INTERVAL_SECONDS");
  eventHandler.orderRefreshIntervalOffsetSeconds = UtilSystem::getEnvAsInt("ORDER_REFRESH_INTERVAL_OFFSET_SECONDS") % eventHandler.orderRefreshIntervalSeconds;
  eventHandler.accountBalanceRefreshWaitSeconds = UtilSystem::getEnvAsInt("ACCOUNT_BALANCE_REFRESH_WAIT_SECONDS");
  eventHandler.accountId = UtilSystem::getEnvAsString("ACCOUNT_ID");
  if (eventHandler.exchange == "coinbase") {
    eventHandler.useGetAccountsToGetAccountBalances = true;
  }
  SessionOptions sessionOptions;
  sessionOptions.httpConnectionPoolIdleTimeoutMilliSeconds = 1 + eventHandler.accountBalanceRefreshWaitSeconds;
  SessionConfigs sessionConfigs;
  Session session(sessionOptions, sessionConfigs, &eventHandler);
  Request request(Request::Operation::GET_INSTRUMENT, eventHandler.exchange, eventHandler.instrument, "GET_INSTRUMENT");
  session.sendRequest(request);
  while (true) {
    std::this_thread::sleep_for(std::chrono::seconds(INT_MAX));
  }
  return EXIT_SUCCESS;
}
// if loss is large then quick_exit which can register hooks
// 50% order size is a special case for ping pong
