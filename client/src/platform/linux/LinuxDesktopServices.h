#pragma once

#include "ports/IPlatformServices.h"

namespace wim::client {

class LinuxDesktopServices final : public IPlatformServices {
 public:
  QString PlatformName() const override;
  bool DesktopNotificationsAvailable() const override;
  bool ShowDesktopNotification(const QString &title,
                               const QString &body) override;
};

}  // namespace wim::client
