#ifndef KUBECTLRUNNER_H
#define KUBECTLRUNNER_H

#include <QObject>
#include <QProcess>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QVariantList>
#include <QTimer>
#include <QCoreApplication>
#include <QProcessEnvironment>
#include <QDebug>

class KubectlRunner : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantList podList READ podList NOTIFY podListChanged)
    Q_PROPERTY(bool k3sRunning READ k3sRunning NOTIFY k3sStatusChanged)

public:
    explicit KubectlRunner(QObject *parent = nullptr) : QObject(parent) {}

    bool k3sRunning() const { return m_k3sRunning; }
    QVariantList podList() const { return m_podList; }

    Q_INVOKABLE void startK3s() {
        // Use detached so the script lives on even if the dashboard blips
        QProcess::startDetached("/bin/bash", {"/home/root/start_k3s.sh"});
        // Give K3s time to initialize the API server before refreshing
        QTimer::singleShot(8000, this, &KubectlRunner::refresh);
    }

    Q_INVOKABLE void refresh() {
        // Start with the standard k3s location on the reMarkable
        runKubectl("/etc/rancher/k3s/k3s.yaml"); 
    }

    Q_INVOKABLE void exitToSystem() {
        qDebug() << "Exiting dashboard and restoring xochitl...";
        
        // 1. Optional: stop k3s if you want to save battery/resources
        QProcess::execute("/home/root/stop_k3s.sh");
        
        // 2. Restart xochitl. Use startDetached so it persists after this app dies.
        QProcess::startDetached("systemctl", {"start", "xochitl"});
        QProcess::startDetached("/home/root/watchme.sh", {"start", "xochitl"});

        
        // 3. Exit this app
        QCoreApplication::quit();
    }

signals:
    void k3sStatusChanged();
    void podListChanged();

private:
    void runKubectl(const QString &configPath) {
        QProcess *proc = new QProcess(this);
        QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
        
        // Ensure kubectl knows where to look
        env.insert("KUBECONFIG", configPath);
        // Force basic output to avoid terminal styling codes in the JSON
        env.insert("TERM", "dumb"); 
        proc->setProcessEnvironment(env);

        proc->start("kubectl", {"get", "pods", "-A", "-o", "json"});
        
        connect(proc, &QProcess::finished, [this, proc, configPath]() {
            bool success = (proc->exitStatus() == QProcess::NormalExit && proc->exitCode() == 0);
            
            if (success) {
                m_k3sRunning = true;
                parsePods(proc->readAllStandardOutput());
            } else {
                qWarning() << "Kubectl failed with error:" << proc->readAllStandardError();
                m_k3sRunning = false;
                m_podList.clear();
            }
            
            updateUI();
            proc->deleteLater();
        });
    }

    void updateUI() {
        emit k3sStatusChanged();
        emit podListChanged();
    }

    void parsePods(const QByteArray &data) {
        QJsonDocument doc = QJsonDocument::fromJson(data);
        if (doc.isNull() || !doc.isObject()) return;

        QJsonArray items = doc.object()["items"].toArray();
        QVariantList newList;

        for (const QJsonValue &value : items) {
            QJsonObject obj = value.toObject();
            QVariantMap pod;
            
            // Extract Metadata
            QJsonObject metadata = obj["metadata"].toObject();
            pod["namespace"] = metadata["namespace"].toString();
            pod["name"] = metadata["name"].toString();
            
            // Extract Status
            QJsonObject statusObj = obj["status"].toObject();
            pod["status"] = statusObj["phase"].toString();
            
            // Calculate Ready Count
            QJsonArray containerStatuses = statusObj["containerStatuses"].toArray();
            int readyCount = 0;
            int totalCount = containerStatuses.size();
            
            for(const QJsonValue &cs : containerStatuses) {
                if(cs.toObject()["ready"].toBool()) {
                    readyCount++;
                }
            }
            
            // Format as "1/1" etc.
            pod["ready"] = QString("%1/%2").arg(readyCount).arg(totalCount > 0 ? totalCount : 0);
            
            newList.append(pod);
        }
        m_podList = newList;
    }

    bool m_k3sRunning = false;
    QVariantList m_podList;
};

#endif // KUBECTLRUNNER_H