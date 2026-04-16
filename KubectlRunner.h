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
#include <QSysInfo>
#include <QDebug>
#include <QDir>
#include <QStandardPaths>

class KubectlRunner : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantList podList READ podList NOTIFY podListChanged)
    Q_PROPERTY(bool k3sRunning READ k3sRunning NOTIFY k3sStatusChanged)
    Q_PROPERTY(QString hostname READ hostname CONSTANT)

public:
    explicit KubectlRunner(QObject *parent = nullptr) : QObject(parent) {
        if (!checkK3sProcess()) {
            qDebug() << "k3s not detected. Initializing startup sequence...";
            startK3s();
        } else {
            m_k3sRunning = true;
            refresh();
        }
    }

    bool k3sRunning() const { return m_k3sRunning; }
    QVariantList podList() const { return m_podList; }
    QString hostname() const { return QSysInfo::machineHostName(); }

    Q_INVOKABLE void startK3s() {
        QString logPath = "/home/root/k3s.log";
        m_k3sRunning = true;
        emit k3sStatusChanged();

        // 1. Load kernel modules required for K3s networking
        // These are blocking calls, but they are very fast.
        QProcess::execute("modprobe", {"nf_tables"});
        QProcess::execute("modprobe", {"nf_tables_ipv4"});
        QProcess::execute("modprobe", {"nft_compat"});

        // 2. Define the server arguments
        QString k3sBin = "/home/root/sbin/k3s";
        QStringList args;
        args << "server"
             << "--data-dir" << "/home/root/k3s_data"
             << "--disable" << "local-storage"
             << "--disable" << "metrics-server"
             << "--disable-cloud-controller"
             << "--kube-apiserver-arg=max-requests-inflight=10"
             << "--kube-apiserver-arg=max-mutating-requests-inflight=5"
             << "--kubelet-arg=enforce-node-allocatable=pods"
             << "--snapshotter=native"
             << "--flannel-backend=host-gw";

        // 3. Launch K3s as a detached process
        // This replaces nohup/setsid and redirects logs to a file manually
        // We wrap it in a shell to handle the log redirection easily
        if (!QDir("/home/root").exists()) {
            logPath = QDir::currentPath() + "/k3s.log";
        }
        QString command = QString("%1 %2 > %3 2>&1").arg(k3sBin).arg(args.join(" ")).arg(logPath);        

        if (QProcess::startDetached("/bin/sh", {"-c", command})) {
            qDebug() << "k3s server process launched.";
            QTimer::singleShot(8000, this, &KubectlRunner::refresh);
        } else {
            qWarning() << "Failed to launch k3s binary.";
            m_k3sRunning = false;
            emit k3sStatusChanged();
        }
    }

    Q_INVOKABLE void refresh() {
        QString config = "/etc/rancher/k3s/k3s.yaml";
    
        // If testing locally, try to use your default local kubeconfig
        if (!QFile::exists(config)) {
            config = QDir::homePath() + "/.kube/config";
        }
        runKubectl(config); 
    }

    Q_INVOKABLE void exitToSystem() {
        qDebug() << "Shutting down inkctl...";
        
        // 1. Stop k3s (Replaces stop_k3s.sh)
        stopK3s();

        // 2. Restart xochitl
        QProcess::startDetached("systemctl", {"start", "xochitl"});
        
        QCoreApplication::quit();
    }

signals:
    void k3sStatusChanged();
    void podListChanged();

private:
    bool checkK3sProcess() {
        QProcess check;
        check.start("pgrep", {"-f", "k3s server"});
        check.waitForFinished(1000); 
        return (check.exitCode() == 0);
    }

    void stopK3s() {
        // Find the PID and kill it, then unmount any leftover k3s mounts
        QProcess::execute("pkill", {"-f", "k3s"});
        
        // Cleanup mounts that k3s often leaves behind
        // This is a common requirement on the reMarkable to prevent disk issues
        QProcess::execute("/bin/sh", {"-c", "grep -l 'k3s' /proc/self/mounts | xargs -r umount"});
    }

    void runKubectl(const QString &configPath) {
        QProcess *proc = new QProcess(this);
        QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
        env.insert("KUBECONFIG", configPath);
        env.insert("TERM", "dumb"); 
        proc->setProcessEnvironment(env);
    
        // 1. Try to find 'kubectl' in the system PATH (works for local dev)
        QString kubectlBin = QStandardPaths::findExecutable("kubectl");

        // 2. If not found, fallback to the reMarkable-specific k3s wrapper
        if (kubectlBin.isEmpty()) {
            kubectlBin = "/home/root/sbin/k3s";
            // When using the k3s wrapper, the first argument must be "kubectl"
            proc->start(kubectlBin, {"kubectl", "get", "pods", "-A", "-o", "json"});
        } else {
            // When using a standalone kubectl, just pass the arguments
            proc->start(kubectlBin, {"get", "pods", "-A", "-o", "json"});
        }        
        connect(proc, &QProcess::finished, [this, proc]() {
            bool success = (proc->exitStatus() == QProcess::NormalExit && proc->exitCode() == 0);
            if (success) {
                m_k3sRunning = true;
                parsePods(proc->readAllStandardOutput());
            } else {
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
            QJsonObject metadata = obj["metadata"].toObject();
            pod["namespace"] = metadata["namespace"].toString();
            pod["name"] = metadata["name"].toString();
            QJsonObject statusObj = obj["status"].toObject();
            pod["status"] = statusObj["phase"].toString();
            
            QJsonArray containerStatuses = statusObj["containerStatuses"].toArray();
            int readyCount = 0;
            for(const QJsonValue &cs : containerStatuses) {
                if(cs.toObject()["ready"].toBool()) readyCount++;
            }
            
            pod["ready"] = QString("%1/%2").arg(readyCount).arg(containerStatuses.size());
            newList.append(pod);
        }
        m_podList = newList;
    }

    bool m_k3sRunning = false;
    QVariantList m_podList;
};

#endif // KUBECTLRUNNER_H