#pragma once

#include "ports/IClientRepository.h"

namespace wim::client {

class FakeScenarioRepository final : public IClientRepository {
 public:
  QStringList ScenarioNames() const override;
  ClientSnapshot LoadScenario(const QString &scenarioName) const override;

 private:
  static ClientSnapshot NormalScenario(const QString &scenarioName);
};

}  // namespace wim::client
