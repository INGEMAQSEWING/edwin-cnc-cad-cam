// Escribe el plano.json (muros, vanos y muebles) de las casas de los tutoriales, igual que
// lo arma DuctusArq 2D, para probar el render con Blender:  plano_json colombia|miami
#define main generar_main
#include "generar_figuras.cpp"
#undef main
#include "muebles.h"
#include "ventas.h"
int main(int argc,char**argv){
  const bool mia = argc>1 && std::string(argv[1])=="miami";
  SIS= mia?Sistema::Imperial:Sistema::Metrico; UN=Unidades{SIS, mia?48.0:50.0};
  Ej e= mia?miami():colombia(); CasaGeom g=casaGeom(e.casa);
  std::vector<Rect> todo=g.muros; todo.insert(todo.end(),g.columnas.begin(),g.columnas.end());
  std::vector<Seg> muros=contornoUnion(todo);
  const std::vector<Vano> vanos=vanosListos(e,g,muros);
  std::printf("{\"unidad_m\":%s,\"alto_muro\":%s,\"muros\":[", mia?"0.0254":"1", mia?"108":"2.4");
  bool pri=true; for(auto&r:todo){ auto q=esquinas(r); std::printf("%s[[%f,%f],[%f,%f],[%f,%f],[%f,%f]]",pri?"":",",q[0].x,q[0].y,q[1].x,q[1].y,q[2].x,q[2].y,q[3].x,q[3].y); pri=false; }
  std::printf("],\"vanos\":["); pri=true;
  for(size_t i=0;i<vanos.size();i++){ const Vano&v=vanos[i];
    const double alto = i<e.vanos.size()? e.vanos[i].alto : UN.metros(2.10), ante = i<e.vanos.size()? e.vanos[i].antepecho : 0;
    P a=local(v,0,0),b=local(v,v.ancho,0),c=local(v,v.ancho,v.espesor),dd=local(v,0,v.espesor);
    std::printf("%s{\"tipo\":\"%s\",\"alto\":%f,\"antepecho\":%f,\"esquinas\":[[%f,%f],[%f,%f],[%f,%f],[%f,%f]]}",pri?"":",",nombreTipoVano(v.tipo),alto,ante,a.x,a.y,b.x,b.y,c.x,c.y,dd.x,dd.y); pri=false; }
  std::printf("],\"muebles\":["); pri=true;
  mueblesEjemplo(mia,[&](const char*id,double x,double y,double gr){ const Pieza*p=buscarPieza(id);
    double w=mia?p->anchoIn:p->anchoM,f=mia?p->fondoIn:p->fondoM,h=mia?p->altoIn:p->altoM;
    std::printf("%s{\"id\":\"%s\",\"x\":%f,\"y\":%f,\"grados\":%f,\"ancho\":%f,\"fondo\":%f,\"alto\":%f}",pri?"":",",id,x,y,gr,w,f,h); pri=false;});
  std::printf("],\"espacios\":["); pri=true;
  { std::vector<Seg> cierre=muros;
    for(const Vano&v:vanos) cierreVano("DUCTUS-VANO|P|X|1|1|0||"+esquinasVano(v),cierre);
    static const char*tipos[]={"calido","bano","cocina","closet","exterior","vacio"};
    for(auto&x:e.espacios){ AreaEspacio a=areaPorPunto(cierre,x.punto,UN.metros(200),UN.metros(0.001)); if(!a.cerrado) continue;
      std::printf("%s{\"nombre\":\"%s\",\"tipo\":\"%s\",\"celdas\":[",pri?"":",",x.nombre,tipos[(int)pisoPorNombre(x.nombre)]); pri=false;
      for(size_t k=0;k<a.celdas.size();k++){auto&r=a.celdas[k]; std::printf("%s[%f,%f,%f,%f]",k?",":"",r.x1,r.y1,r.x2,r.y2);} std::printf("]}"); } }
  std::printf("]}\n");
}
