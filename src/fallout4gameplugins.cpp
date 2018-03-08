#include "fallout4gameplugins.h"
#include <safewritefile.h>
#include <report.h>
#include <ipluginlist.h>
#include <report.h>
#include <scopeguard.h>

#include <QDir>
#include <QTextCodec>
#include <QStringList>
#include <QSet>


using MOBase::IPluginGame;
using MOBase::IPluginList;
using MOBase::IOrganizer;
using MOBase::SafeWriteFile;
using MOBase::reportError;

Fallout4GamePlugins::Fallout4GamePlugins(IOrganizer *organizer)
  : GamebryoGamePlugins(organizer)
{
}

void Fallout4GamePlugins::getLoadOrder(QStringList &loadOrder) {
  QString loadOrderPath =
    organizer()->profile()->absolutePath() + "/loadorder.txt";
  QString pluginsPath = organizer()->profile()->absolutePath() + "/plugins.txt";

  bool loadOrderIsNew = !m_LastRead.isValid() ||
    !QFileInfo(loadOrderPath).exists() ||
    QFileInfo(loadOrderPath).lastModified() > m_LastRead;
  bool pluginsIsNew = !m_LastRead.isValid() ||
    QFileInfo(pluginsPath).lastModified() > m_LastRead;

  if (loadOrderIsNew || !pluginsIsNew) {
    loadOrder = readLoadOrderList(m_Organizer->pluginList(), loadOrderPath);
  }
  else {
    loadOrder = readPluginList(m_Organizer->pluginList(), pluginsPath);
  }
}

void Fallout4GamePlugins::writePluginList(const IPluginList *pluginList,
                                          const QString &filePath) {
  SafeWriteFile file(filePath);

  QTextCodec *textCodec = localCodec();

  file->resize(0);

  file->write(textCodec->fromUnicode(
      "# This file was automatically generated by Mod Organizer.\r\n"));

  bool invalidFileNames = false;
  int writtenCount = 0;

  QStringList plugins = pluginList->pluginNames();
  std::sort(plugins.begin(), plugins.end(),
            [pluginList](const QString &lhs, const QString &rhs) {
              return pluginList->priority(lhs) < pluginList->priority(rhs);
            });

  QStringList PrimaryPlugins = organizer()->managedGame()->primaryPlugins();
  QSet<QString> ManagedMods = PrimaryPlugins.toSet().subtract(organizer()->managedGame()->DLCPlugins().toSet());
  PrimaryPlugins.append(ManagedMods.toList());

  //TODO: do not write plugins in OFFICIAL_FILES container
  for (const QString &pluginName : plugins) {
	if (!PrimaryPlugins.contains(pluginName,Qt::CaseInsensitive)) {
    if (pluginList->state(pluginName) == IPluginList::STATE_ACTIVE) {
      if (!textCodec->canEncode(pluginName)) {
        invalidFileNames = true;
        qCritical("invalid plugin name %s", qPrintable(pluginName));
      }
      else
      {
        file->write("*");
        file->write(textCodec->fromUnicode(pluginName));

      }
      file->write("\r\n");
      ++writtenCount;
    }
	  else
	  {
        if (!textCodec->canEncode(pluginName)) {
          invalidFileNames = true;
          qCritical("invalid plugin name %s", qPrintable(pluginName));
        }
        else
        { 
          file->write(textCodec->fromUnicode(pluginName));
        }
        file->write("\r\n");
        ++writtenCount;
	  }
    }
  }

  if (invalidFileNames) {
    reportError(QObject::tr("Some of your plugins have invalid names! These "
                            "plugins can not be loaded by the game. Please see "
                            "mo_interface.log for a list of affected plugins "
                            "and rename them."));
  }

  if (file.commitIfDifferent(m_LastSaveHash[filePath])) {
    qDebug("%s saved", qPrintable(QDir::toNativeSeparators(filePath)));
  }
}

QStringList Fallout4GamePlugins::readPluginList(MOBase::IPluginList *pluginList,
                                         const QString &filePath)
{
  QStringList plugins = pluginList->pluginNames();
  QStringList primaryPlugins = organizer()->managedGame()->primaryPlugins();
  QStringList loadOrder(primaryPlugins);

  for (const QString &pluginName : loadOrder) {
    if (pluginList->state(pluginName) != IPluginList::STATE_MISSING) {
      pluginList->setState(pluginName, IPluginList::STATE_ACTIVE);
    }
  }

  QString filePath = organizer()->profile()->absolutePath() + "/plugins.txt";
  QFile file(filePath);
  if (!file.open(QIODevice::ReadOnly)) {
    qWarning("%s not found", qPrintable(filePath));
    return loadOrder;
  }
  ON_BLOCK_EXIT([&]() { file.close(); });

  if (file.size() == 0) {
    // MO stores at least a header in the file. if it's completely empty the
    // file is broken
    qWarning("%s empty", qPrintable(filePath));
    return loadOrder;
  }

  while (!file.atEnd()) {
    QByteArray line = file.readLine();
    QString pluginName;
    if ((line.size() > 0) && (line.at(0) != '#')) {
      pluginName = localCodec()->toUnicode(line.trimmed().constData());
    }
	if (!primaryPlugins.contains(pluginName, Qt::CaseInsensitive)) {
		if (pluginName.startsWith('*')) {
			pluginName.remove(0, 1);
			if (pluginName.size() > 0) {
				pluginList->setState(pluginName, IPluginList::STATE_ACTIVE);
				plugins.removeAll(pluginName);
				if (!loadOrder.contains(pluginName, Qt::CaseInsensitive)) {
					loadOrder.append(pluginName);
				}
			}
		}
		else
		{
			if (pluginName.size() > 0) {
				pluginList->setState(pluginName, IPluginList::STATE_INACTIVE);
				plugins.removeAll(pluginName);
				if (!loadOrder.contains(pluginName, Qt::CaseInsensitive)) {
					loadOrder.append(pluginName);
				}
			}
		}
	}
	else
	{
		pluginName.remove(0, 1);
		plugins.removeAll(pluginName);
	}
  }

  file.close();

  // we removed each plugin found in the file, so what's left are inactive mods
  for (const QString &pluginName : plugins) {
    pluginList->setState(pluginName, IPluginList::STATE_INACTIVE);
  }

  return loadOrder;
}
