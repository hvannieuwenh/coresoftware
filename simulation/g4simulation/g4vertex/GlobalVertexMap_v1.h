#ifndef __GLOBALVERTEXMAP_V1_H__
#define __GLOBALVERTEXMAP_V1_H__

#include "GlobalVertexMap.h"
#include "GlobalVertex.h"

#include <phool/PHObject.h>
#include <map>
#include <iostream>

class GlobalVertexMap_v1 : public GlobalVertexMap {
  
public:

  GlobalVertexMap_v1();
  virtual ~GlobalVertexMap_v1();

  void identify(std::ostream &os = std::cout) const;
  void Reset() {clear();}
  int  isValid() const {return 1;}
  
  bool   empty()                   const {return _map.empty();}
  size_t size()                    const {return _map.size();}
  size_t count(unsigned int idkey) const {return _map.count(idkey);}
  void   clear();
  
  const GlobalVertex* get(unsigned int idkey) const;
        GlobalVertex* get(unsigned int idkey); 
        GlobalVertex* insert(GlobalVertex* vertex);
        size_t        erase(unsigned int idkey) {
    delete _map[idkey];
    return _map.erase(idkey);
  }

  ConstIter begin()                   const {return _map.begin();}
  ConstIter  find(unsigned int idkey) const {return _map.find(idkey);}
  ConstIter   end()                   const {return _map.end();}

  Iter begin()                   {return _map.begin();}
  Iter  find(unsigned int idkey) {return _map.find(idkey);}
  Iter   end()                   {return _map.end();}
  
private:
  std::map<unsigned int, GlobalVertex*> _map;
    
  ClassDef(GlobalVertexMap_v1, 1);
};

#endif // __GLOBALVERTEXMAP_V1_H__
