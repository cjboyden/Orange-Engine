#include "mapscriptdialog.h"
#include "scripttab.h"
#include "map.h"

MapScriptDialog::MapScriptDialog(Map * m) :
    ScriptDialog()
{
  map = m;

  setWindowTitle("Map Scripts");
  for(int i = 0; i < map->getScriptCount(); i++) {
    //message(QString::number(i) + ": " + entity->getScript(i));
    ScriptTab * scriptTab = new ScriptTab(map->getScriptCondition(i), map->getScript(i));
    scriptTabs->addTab(scriptTab, QString::number(i + 1));
  }
}

int MapScriptDialog::exec() {
  int r = QDialog::exec();

  if(r == QDialog::Accepted) {
    // Save changes to map
    map->clearScripts();
    for(int i = 0; i < scriptTabs->count(); i++) {
      ScriptTab * widget = dynamic_cast<ScriptTab *> (scriptTabs->widget(i));
      if(widget) {
        int condition = widget->conditionSelect->currentIndex();
        QString script = widget->scriptText->toPlainText();

        map->addScript(condition, script);
      } else {
        message("ScriptDialog: tab is not of type ScriptTab");
      }
    }
  }

  return r;
}
