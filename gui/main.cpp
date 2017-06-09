/*
 * Copyright (C) 2015-2017 Olzhas Rakhimov
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/// @file main.cpp
/// The main entrance to the SCRAM GUI.

#include <exception>
#include <iostream>
#include <string>
#include <vector>

#include <QApplication>
#include <QCoreApplication>
#include <QIcon>
#include <QMessageBox>
#include <QString>

#include <boost/program_options.hpp>

#include "mainwindow.h"

#include "src/error.h"
#include "src/version.h"

namespace po = boost::program_options;

namespace {

/**
 * Parses the command-line arguments.
 *
 * @param[in] argc  Count of arguments.
 * @param[in] argv  Values of arguments.
 * @param[out] vm  Variables map of program options.
 *
 * @returns 0 for success.
 * @returns 1 for errored state.
 * @returns -1 for information only state like help and version.
 */
int parseArguments(int argc, char *argv[], po::variables_map *vm) noexcept
{
    const char* usage = "Usage:    scram-gui [options] [input-files]...";
    po::options_description desc("Options");
    desc.add_options()
            ("help", "Display this help message")
            ("config-file", po::value<std::string>()->value_name("path"),
             "Project configuration file");
    try {
        po::store(po::parse_command_line(argc, argv, desc), *vm);
    } catch (std::exception &err) {
        std::cerr << "Option error: " << err.what() << "\n\n"
                  << usage << "\n\n"
                  << desc << "\n";
        return 1;
    }

    po::notify(*vm);

    // Process command-line arguments.
    if (vm->count("help")) {
        std::cout << usage << "\n\n" << desc << "\n";
        return -1;
    }
    desc.add_options()("input-files", po::value<std::vector<std::string>>());
    po::positional_options_description p;
    p.add("input-files", -1);

    po::store(
        po::command_line_parser(argc, argv).options(desc).positional(p).run(),
        *vm);
    po::notify(*vm);
    return 0;
}

/// Guards the application from crashes on escaped internal exceptions.
class GuardedApplication : public QApplication {
public:
    using QApplication::QApplication;

    bool notify(QObject *receiver, QEvent *event) override
    {
        try {
            return QApplication::notify(receiver, event);
        } catch (const scram::Error &err) {
            qCritical("%s", err.what());
            QMessageBox::critical(nullptr, tr("Internal SCRAM Error"),
                                  QString::fromUtf8(err.what()));
        } catch (const std::exception &err) {
            qCritical("%s", err.what());
            QMessageBox::critical(nullptr, tr("Unexpected Error"),
                                  QString::fromUtf8(err.what()));
        }
        return false;
    }
};

} // namespace

int main(int argc, char *argv[])
{
    // Keep the following commented code!
    // In some static build configurations,
    // the resources may fail to load.
    // However, the most distributions are expected to be shared builds,
    // so the explicit load should not be used, but it is kept for debugging.
    /* Q_INIT_RESOURCE(res); */

    QCoreApplication::setOrganizationName(QString::fromLatin1("scram"));
    QCoreApplication::setOrganizationDomain(
        QString::fromLatin1("scram-pra.org"));
    QCoreApplication::setApplicationName(QString::fromLatin1("scram"));
    QCoreApplication::setApplicationVersion(
        QString::fromLatin1(scram::version::core()));

    GuardedApplication a(argc, argv);

    if (QIcon::themeName().isEmpty())
        QIcon::setThemeName(QStringLiteral("tango"));

    scram::gui::MainWindow w;
    w.show();

    if (argc > 1) {
        po::variables_map vm;
        int ret = parseArguments(argc, argv, &vm);
        if (ret == 1)
            return 1;
        if (ret == -1)
            return 0;
        std::vector<std::string> inputFiles;
        if (vm.count("input-files"))
            inputFiles = vm["input-files"].as<std::vector<std::string>>();
        if (vm.count("config-file")) {
            w.setConfig(vm["config-file"].as<std::string>(), inputFiles);
        } else {
            w.addInputFiles(inputFiles);
        }
    }
    return a.exec();
}
