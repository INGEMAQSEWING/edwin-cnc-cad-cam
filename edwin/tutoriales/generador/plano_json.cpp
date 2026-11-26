// Escribe el plano.json (muros, vanos y muebles) de las casas de los tutoriales, igual que
// lo arma DuctusArq 2D, para probar el render con Blender:  plano_json colombia|miami
#define main generar_main
#include "generar_figuras.cpp"
#undef main
#include "muebles.h"
int main(int argc,char**argv){
  const bool mia = argc>1 && std::string(argv[1])=="miami";
  SIS= mia?Sistema::Imperial:Sistema::Metrico; UN=Unidades{SIS, mia?48.0:50.0};
  Ej e= mia?miami():colombia(); CasaGeom g=casaGeom(e.casa);
  std::vector<Rect> todo=g.muros; todo.insert(todo.end(),g.columnas.begin(),g.columnas.end());
  std::vector<Seg> muros=contornoUnion(todo);
  std::printf("{\"unidad_m\":%s,\"alto_muro\":%s,\"muros\":[", mia?"0.0254":"1", mia?"108":"2.4");
  bool pri=true; for(auto&r:todo){ auto q=esquinas(r); std::printf("%s[[%f,%f],[%f,%f],[%f,%f],[%f,%f]]",pri?"":",",q[0].x,q[0].y,q[1].x,q[1].y,q[2].x,q[2].y,q[3].x,q[3].y); pri=false; }
  std::printf("],\"vanos\":["); pri=true;
  for(auto&d:e.vanos){ Vano v; v.tipo=d.tipo; v.o=d.o; v.dir=d.dir; v.ancho=d.ancho; double w=0;
    buscarOtraCara(muros,v.o,v.dir,v.ancho,UN.metros(0.8),UN.metros(0.001),w); P perp(-v.dir.y,v.dir.x); v.n= w>0?perp:perp*-1.0; v.espesor=std::fabs(w);
    P a=local(v,0,0),b=local(v,v.ancho,0),c=local(v,v.ancho,v.espesor),dd=local(v,0,v.espesor);
    std::printf("%s{\"tipo\":\"%s\",\"alto\":%f,\"antepecho\":%f,\"esquinas\":[[%f,%f],[%f,%f],[%f,%f],[%f,%f]]}",pri?"":",",nombreTipoVano(d.tipo),d.alto,d.antepecho,a.x,a.y,b.x,b.y,c.x,c.y,dd.x,dd.y); pri=false; }
  std::printf("],\"muebles\":["); pri=true;
  auto m=[&](const char*id,double x,double y,double gr){ const Pieza*p=buscarPieza(id);
    double w=mia?p->anchoIn:p->anchoM,f=mia?p->fondoIn:p->fondoM,h=mia?p->altoIn:p->altoM;
    std::printf("%s{\"id\":\"%s\",\"x\":%f,\"y\":%f,\"grados\":%f,\"ancho\":%f,\"fondo\":%f,\"alto\":%f}",pri?"":",",id,x,y,gr,w,f,h); pri=false;};
  if(!mia){
    m("sofa3",1.75,2.45,0); m("mesa_centro",1.75,1.55,0); m("mueble_tv",1.75,0.40,180);
    m("meson",5.60,0.45,180); m("estufa",6.40,1.50,90); m("nevera",6.45,2.50,90); m("comedor4",4.60,2.00,0);
    m("cama_doble",1.75,5.40,0); m("mesa_noche",0.70,6.20,0); m("closet",3.00,4.40,90);
    m("sanitario",6.40,5.80,270); m("lavamanos",4.60,6.20,0); m("ducha",6.30,3.65,0); m("lavadora",3.90,3.45,180);
    m("cama_sencilla",1.20,8.80,0); m("escritorio",2.70,7.30,180); m("closet",0.50,7.50,270);
    m("cama_queen",5.15,8.85,0); m("mesa_noche",4.05,9.60,0); m("mesa_noche",6.25,9.60,0); m("closet",5.15,6.90,180);
  } else {
    m("cama_doble",92,103,0); m("mesa_noche",40,128,0); m("comoda",92,19,180);
    m("tina",25,210,90); m("lavamanos_doble",110,261,0); m("sanitario",150,161,180);
    m("cama_queen",92,364,0); m("mesa_noche",40,393,0); m("mesa_noche",144,393,0);
    m("cama_king",92,527,0); m("mesa_noche",36,557,0); m("mesa_noche",148,557,0); m("poltrona",40,440,0);
    m("cama_doble",315,103,0); m("escritorio",410,30,180);
    m("sofa3",315,254,0); m("mesa_centro",315,205,0); m("mueble_tv",315,156,180);
    m("lavadora",430,551,0); m("secadora",400,551,0); m("sofa2",300,430,180); m("tv",300,565,0);
  }
  std::printf("]}\n");
}
