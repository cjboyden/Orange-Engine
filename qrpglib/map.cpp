#ifdef WIN32
#include <windows.h>
#endif

#include "map.h"
#include "globals.h"
#include "entity.h"
#include "npc.h"
#include "player.h"
#include "rpgscript.h"
#include <GL/gl.h>
#include <stdlib.h>
#include <iostream>
#include <fstream>
#include <iomanip>
#include <string.h>
#include <QXmlStreamReader>
#include <QtGui>
#include <QGLWidget>

using namespace std;

Map::Map(Bitmap * t, int x, int y, int w, int h, QString mapname) : QObject() {
  tileset = t;
  view_x = x;
  view_y = y;
  view_w = w;
  view_h = h;
  starting = true;
  if(tileset) tileset->getSize(tile_w, tile_h);
  name = mapname;

  maps.push_back(this);
  mapnames[name] = maps.size() - 1;
  thisMap = 
    new Resource(Resource::Map,
	   maps.size() - 1, 
	   name,
	   mapfolder);

  scriptObject = scriptEngine->newQObject(this);
}

Map::Map()
{
  tileset = 0;
  view_x = view_y = view_w = view_h = 0;
  name = "Unnamed Map";
  starting = true;

  maps.push_back(this);
  mapnames[name] = maps.size() - 1;
  thisMap = 
    new Resource(Resource::Map,
	   maps.size() - 1, 
	   name,
	   mapfolder);

  scriptObject = scriptEngine->newQObject(this);
}

bool Map::setName(QString n)
{
  if(mapnames.contains(n)) {
    return false;
  }
  
  mapnames[n] = mapnames[name];
  mapnames.remove(name);
  name = n;
  thisMap->setText(0, name);
  return true;
}

QString Map::getName()
{
  return name;
}

QString Map::getLayerName(int layer) {
  return layers[layer]->name;
}

void Map::setLayerName(int layer, QString name) {
  layers[layer]->name = name;
}

Map::Tile::~Tile() {
  int i;
  for(i = 0; i < border.size(); i++) delete border[i];
}

Map::Layer::~Layer() {
  int i;
  for(i = 0; i < border.size(); i++) delete border[i];
  if(layerdata) delete layerdata;
}

Map::Layer::Layer() {
  layerdata = 0;
  tileset = 0;
}
  
Map::Layer::Layer(int h, int w, int fill) {
  width = w;
  height = h;
  layerdata = new int[h*w];
  wrap = false;

  for(int i = 0; i < h * w; i++) layerdata[i] = fill;
}

Map::Layer::Layer(Layer * l, int xo, int yo, int w, int h, int fill) {
  width = w;
  height = h;
  layerdata = new int[h*w];
  wrap = false;

  for(int x = 0; x < width; x++) {
    for(int y = 0; y < height; y++) {
      if(x + xo >= 0 && y + yo >= 0 && x + xo < l->width && y + yo < l->height) {
        layerdata[x + y * w] = l->layerdata[x + xo + (y + yo) * l->width];
      } else {
        layerdata[x + y * w] = fill;
      }
    }
  }
}

Map::Layer::Layer(Layer * l) {
  width = l->width;
  height = l->height;
  layerdata = new int[height*width];
  wrap = l->wrap;

  for(int x = 0; x < width; x++) {
    for(int y = 0; y < height; y++) {
      layerdata[x + y * width] = l->layerdata[x + y * width];
    }
  }
}

void Map::Layer::clear(int fill) {
  fillArea(0, 0, width, height, fill);
}

void Map::Layer::stamp(Layer * l, int xo, int yo, int x_offset, int y_offset, bool skipZero) {
  for(int x = x_offset; x < width; x++) {
    for(int y = y_offset; y < height; y++) {
      if(x + xo >= 0 && y + yo >= 0 && x + xo < l->width && y + yo < l->height) {
        int t = layerdata[x + y * width];
        if(!skipZero || t > 0) l->layerdata[x + xo + (y + yo) * l->width] = t;
      }
    }
  }
}

void Map::Layer::resize(int w, int h, int fill) {
  int * newdata = new int[h*w];
  for(int i = 0; i < h * w; i++) newdata[i] = fill;

  for(int x = 0; x < width; x++) {
    for(int y = 0; y < height; y++) {
      if(x < w && y < h) {
        newdata[x + y * w] = layerdata[x + y * width];
      }
    }
  }

  delete layerdata;
  layerdata = newdata;
  width = w;
  height = h;

  //message("layer resized");
  //dump();
}

void Map::Layer::dump() {
  for(int y = 0; y < height; y++) {
    for(int x = 0; x < width; x++) {
      cout << setw(4) << layerdata[x + y * width] << " ";
    }
    cout << "\n";
  }
}

void Map::Layer::fillArea(int xo, int yo, int w, int h, int fill) {
  for(int x = xo; x < xo + w; x++) {
    for(int y = yo; y < yo + h; y++) {
      if(x < width && y < height) {
        layerdata[x + y * width] = fill;
      }
    }
  }
}

void Map::Layer::runUnLoadScripts() {
  EntityPointer e;
  foreach(e, entities) {
    e->runUnLoadScripts();
  }
}

Map::~Map() {
  // Map scripts
  int i;

  for(i = 0; i < tiles.size(); i++) delete tiles[i];
  for(i = 0; i < layers.size(); i++) delete layers[i];
}
    
void Map::update() {

  int i = 0;

  // Map scripts
  for(i = 0; i < scripts.size(); i++) {
    bool execute = false;
    RPGScript * s = &(scripts[i]);
    if(starting && s->condition == ScriptCondition::Load) {
      execute = true;
    } else if(s->condition == ScriptCondition::EveryFrame) {
      execute = true;
    }

    //push context
    if(execute) {
      //cprint("execute: " + s->script + "\n");
      QScriptContext * context = scriptEngine->pushContext();
      context->setThisObject(scriptObject);
      scriptEngine->evaluate(s->script);

      if(scriptEngine->hasUncaughtException())
        message(scriptEngine->uncaughtException().toString());

      scriptEngine->popContext();
    }
  }

  // update entities
  if(!paused) {
    if(playerEntity->isActivated()) qDebug() << "player activated";
    for(int i = 0; i < layers.size(); i++) {
      for(int j = 0; j < layers[i]->entities.size(); j++) {

        if(playerEntity->isActivated()) qDebug() << i << " " << j << ": " << layers[i]->entities[j]->getName();
        layers[i]->entities[j]->update();
      }
    }
  }

  starting = false;
}

void Map::getTileSize(int &w, int &h) {
  w = tile_w;
  h = tile_h;
}

void Map::getSize(int layer, int &w, int &h) {
  if(layer < layers.size()) {
    w = layers[layer]->width;
    h = layers[layer]->height;
  } else {
    w = 0; 
    h = 0;
  }
}

Bitmap * Map::getTileset() {
  return tileset;
}

void Map::setViewport(int x, int y, int w, int h) {
  view_x = x;
  view_y = y;
  view_w = w;
  view_h = h;
}

void Map::draw(Layer *layer, int x, int y, float opacity, bool boundingboxes, bool entities) {
  if(layer && tileset) {
    int xs, ys, h, w, x_off, y_off;
    int i, j;

    xs = x / tile_w;
    ys = y / tile_h;
    w = view_w / tile_w + 2;
    h = view_h / tile_h + 2;
    x_off = x % tile_w;
    y_off = y % tile_h;
    if(x_off == tile_w) x_off = 0;
    if(y_off == tile_h) y_off = 0;

    for(j = ys; j < ys + h; j++) {
      for(i = xs; i < xs + w; i++) {
        if(i >= 0 && i < layer->width && j >= 0 && j < layer->height) {
          int tile_x = (i - xs) * tile_w - x_off + view_x;
          int tile_y = (j - ys) * tile_h - y_off + view_y;

          tileset->draw(getTile(layer, i, j), tile_x, tile_y, opacity);
        }
      }
    }

    if(entities) {
      // Sort entities in Y direction
      if(play) {
        if(layer->entities.size())
          qSort(layer->entities.begin(), layer->entities.end(), Entity::entity_y_order);

        for(i = 0; i < layer->entities.size(); i++) {
          layer->entities[i]->draw(x, y, 1, boundingboxes);
        }
      } else {
        if(layer->startEntities.size())
          qSort(layer->startEntities.begin(), layer->startEntities.end(), Entity::entity_y_order);

        for(i = 0; i < layer->startEntities.size(); i++) {
          layer->startEntities[i]->draw(x, y, opacity, boundingboxes);
        }
      }
    }

  }
}

void Map::draw(int layer, int x, int y, float opacity, bool boundingboxes, bool entities) {
  draw(layers[layer], x, y, opacity, boundingboxes, entities);
}

void Map::addEntity(int layer, EntityPointer entity) {
  removeEntity(entity->getLayer(), entity);
  if(layer < layers.size()) {
    if(play)
      layers[layer]->entities.push_back(entity);
    else
      layers[layer]->startEntities.push_back(entity);      

    entity->setLayer(layer);
  }
}

void Map::addStartEntity(int layer, EntityPointer entity) {
  layers[layer]->startEntities.push_back(entity);      
}

void Map::removeEntity(int layer, EntityPointer entity) {
  if(layer < layers.size()) {
    if(play)
      layers[layer]->entities.removeAll(entity);
    else
      layers[layer]->startEntities.removeAll(entity);      
  }
}

void Map::removeStartEntity(int layer, EntityPointer entity) {
  layers[layer]->startEntities.removeAll(entity);  
}

int Map::getEntityCount(int layer) {
  if(layer < layers.size())
    return layers[layer]->entities.size();
  return 0;
}

int Map::getStartEntityCount(int layer) {
  if(layer < layers.size())
    return layers[layer]->startEntities.size();
  return 0;
}

EntityPointer Map::getEntity(int layer, int index) {
  if(layer < layers.size() && index < layers[layer]->entities.size())
    return layers[layer]->entities[index];
  return EntityPointer();
}

EntityPointer Map::getStartEntity(int layer, int index) {
  if(layer < layers.size() && index < layers[layer]->startEntities.size())
    return layers[layer]->startEntities[index];
  return EntityPointer();
}


int Map::getTile(int layer, int x, int y) {
  if(layer < layers.size() &&
     x >= 0 && x < layers[layer]->width &&
     y >= 0 && y < layers[layer]->height) 
    return layers[layer]->layerdata[x + y * layers[layer]->width];
  return 0;
}

int Map::getTile(Layer * layer, int x, int y) {
  if(layer &&
     x >= 0 && x < layer->width &&
     y >= 0 && y < layer->height)
    return layer->layerdata[x + y * layer->width];
  return 0;
}

void Map::setTile(int layer,
		  int x, int y, int tile) {
  if(layer < layers.size() &&
     x >= 0 && x < layers[layer]->width &&
     y >= 0 && y < layers[layer]->height) 
    layers[layer]->layerdata[x + y * layers[layer]->width] = tile;
}

int Map::getLayerCount() {
  return layers.size();
}

void Map::setTileset(Bitmap * t) {
  tileset = t;
  tileset->getSize(tile_w, tile_h);
}

void Map::setTileset(int layer, Bitmap * t) {
  layers[layer]->tileset = t;
  t->getSize(layers[layer]->tile_w, layers[layer]->tile_h);
}

int Map::addLayer(int w, int h, bool wrap, int filltile, QString name) {
  int i;

  Layer * l = new Layer;
  l->height = h;
  l->width = w;
  l->wrap = wrap;
  l->name = name;
  l->layerdata = new int[h*w]; //(int *) malloc(h * w * sizeof(int));
  for(i = 0; i < h * w; i++) {
    l->layerdata[i] = filltile;
  }
  
  layers.push_back(l);
  return (int) layers.size() - 1;
}
  
void Map::moveLayer(int oldIndex, int newIndex) {
  layers.move(oldIndex, newIndex);
}

void Map::insertLayerBefore(int layer, int w, int h, bool wrap, int filltile, QString name) {
  int l = addLayer(w, h, wrap, filltile, name);
  moveLayer(l, layer);
}

void Map::insertLayerAfter(int layer, int w, int h, bool wrap, int filltile, QString name) {
  int l = addLayer(w, h, wrap, filltile, name);
  moveLayer(l, layer + 1);
}

void Map::deleteLayer(int layer) {
  Layer * l = layers.takeAt(layer);
  delete l;
}

void Map::save(QString filename) {
  ofstream file(filename.toAscii());
  int i, x, y;
  int s = layers.size();

  file << "<map>\n";
  file << "  <name>" << name.toAscii().data() << "</name>\n";
  file << "  <layers>" << s << "</layers>\n";
  file << "  <tileset>" << getTileset()->getName().toAscii().data() << "</tileset>\n";

  file << "  <scripts>";
  for(int y = 0; y < getScriptCount(); y++) {
    QString xml = scripts[y].toXml(4);
    file << xml.toAscii().data();
  }

  file << "  </scripts>";

  for(i = 0; i < layers.size(); i++) {
    file << "  <layer>\n";
    file << "    <width>" << layers[i]->width << "</width>\n";
    file << "    <height>" << layers[i]->height << "</height>\n";
    file << "    <tileset>" << getTileset()->getName().toAscii().data() << "</tileset>\n";
    file << "    <name>" << layers[i]->name.toAscii().data() << "</name>\n";
    file << "    <layerdata>\n";
    for(y = 0; y < layers[i]->height; y++) {
      file << "      ";
      for(x = 0; x < layers[i]->width; x++) {
        file << setw(4) << layers[i]->layerdata[y*layers[i]->width+x] << " ";
      }
      file << "\n";
    }
    file << "    </layerdata>\n";
    file << "    <entities>\n";
    for(x = 0; x < getStartEntityCount(i); x++) {
      EntityPointer e = getStartEntity(i, x);
      file << e->toXml().toAscii().data();
    }
    file << "    </entities>\n";
    file << "  </layer>\n";
  }
  file << "</map>\n";
  file.close();
}

void Map::reset() {
  clear();
  //qDebug() << "resetting " << this->getName();

  for(int i = 0; i < layers.size(); i++) {
    for(int j = 0; j < layers[i]->startEntities.size(); j++) {
      EntityPointer e = layers[i]->startEntities[j];
      EntityPointer newEntity = e->clone();

      //qDebug() << "adding dynamic entity to map: " << e->getName();

      //layers[i]->entities.append(newEntity);
      e->addToMap(i);
      mapentitynames[e->getName()] = e->getId();
    }
  }
}

void Map::clear() {
  for(int i = 0; i < layers.size(); i++) {
    while(!(layers[i]->entities.isEmpty()))
      layers[i]->entities.takeFirst();

    mapentitynames.clear();
  }
}

void Map::setStarting(bool s) {
  starting = s;
}

void Map::addScript(int cond, QString scr) {
  scripts.append(RPGScript(cond, scr));
}

void Map::clearScripts() {
  scripts.clear();
}

int Map::getScriptCount() const {
  return scripts.size();
}

QString Map::getScript(int index) const {
  return scripts[index].script;
}

int Map::getScriptCondition(int index) const {
  return scripts[index].condition;
}

Map::Layer * Map::getLayer(int l) {
  if(l < layers.size())
    return layers[l];
  else
    return 0;
}

QScriptValue Map::getScriptObject() {
  return scriptObject;
}

void Map::runUnLoadScripts() {
  if(!play) return;

  foreach(Layer * l, layers) {
    l->runUnLoadScripts();
  }

  for(int i = 0; i < scripts.size(); i++) {
    bool execute = false;
    RPGScript * s = &(scripts[i]);
    if(s->condition == ScriptCondition::UnLoad) {
      execute = true;
    }

    //push context
    if(execute) {
      //cprint("execute: " + s->script + "\n");
      QScriptContext * context = scriptEngine->pushContext();
      context->setThisObject(scriptObject);
      scriptEngine->evaluate(s->script);

      if(scriptEngine->hasUncaughtException())
        message(scriptEngine->uncaughtException().toString());

      scriptEngine->popContext();
    }
  }
}

QScriptValue mapConstructor(QScriptContext * context, QScriptEngine * engine) {
  QString name = context->argument(0).toString();
  Map * object = new Map();
  return engine->newQObject(object, QScriptEngine::QtOwnership);
}
