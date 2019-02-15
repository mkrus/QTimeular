/*
  Copyright 2019 Mike Krus

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions
  are met:

  1. Redistributions of source code must retain the above copyright
     notice, this list of conditions and the following disclaimer.

  2. Redistributions in binary form must reproduce the above copyright
     notice, this list of conditions and the following disclaimer in
     the documentation and/or other materials provided with the
     distribution.

  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
  FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
  COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
  BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
  CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
  LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
  ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
  POSSIBILITY OF SUCH DAMAGE.
*/

#ifndef TIMEULARMANAGER_H
#define TIMEULARMANAGER_H

#include <QObject>
#include <QBluetoothLocalDevice>
#include <QBluetoothDeviceDiscoveryAgent>
#include <QBluetoothServiceDiscoveryAgent>
#include <QLowEnergyController>

class TimeularManager : public QObject
{
    Q_OBJECT
    Q_PROPERTY(Status status READ status NOTIFY statusChanged)
    Q_PROPERTY(Orientation orientation READ orientation NOTIFY orientationChanged)
public:
    enum Status {
        Disconneted,
        Connecting,
        Connected,
    };
    Q_ENUMS(Status)

    enum Orientation {
        Vertical,
        Face1,
        Face2,
        Face3,
        Face4,
        Face5,
        Face6,
        Face7,
        Face8
    };
    Q_ENUMS(Orientation)

    explicit TimeularManager(QObject *parent = nullptr);
    ~TimeularManager();

    Status status() const;
    Orientation orientation() const;

public slots:
    void startDiscovery();

signals:
    void statusChanged(Status status);
    void orientationChanged(Orientation orientation);

private:
    void setStatus(Status status);
    void deviceDiscovered(const QBluetoothDeviceInfo &info);
    void addLowEnergyService(const QBluetoothUuid &uuid);
    void deviceConnected();
    void errorReceived(QLowEnergyController::Error);
    void serviceScanDone();
    void deviceDisconnected();
    void serviceStateChanged(QLowEnergyService::ServiceState newState);
    void deviceDataChanged(const QLowEnergyCharacteristic &c, const QByteArray &value);
    void confirmedDescriptorWrite(const QLowEnergyDescriptor &d, const QByteArray &value);

    Status m_status = Disconneted;
    bool m_serviceDiscovered = false;
    QBluetoothDeviceDiscoveryAgent *m_deviceDiscoveryAgent = nullptr;
    QBluetoothServiceDiscoveryAgent *m_serviceDiscoveryAgent = nullptr;
    QLowEnergyService *m_service = nullptr;
    QLowEnergyController *m_controller = nullptr;
    QBluetoothLocalDevice *m_localDevice = nullptr;
    QLowEnergyDescriptor m_notificationDesc;
    Orientation m_orientation = Vertical;
};



#endif // TIMEULARMANAGER_H
