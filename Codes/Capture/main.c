// Aquest codi ha de permetre: 


int main(int argc, char *argv[]) {

    // 0) El FE envia el nom del SV sel·leccionat
    // 0.1) Parsejar les entrades
    // 0.2) Safety Checks - Comprovar que tot funciona correctament (els daemons, comprova connexions amb HW...)  

    // 1) Calcular orbita del SV
    // 1.1) Descarregar el TLE actual i comprovar que el SV existeix
    // 1.2) Calcular l'orbita del SV (SGP4) (si l'òrbita passa de 360 a 0 canviar a -1 0 [vegeu NOTA 2] ) i guardar-la en un struct de l'estil:
    /*
    struct SV_orbit{
        timestamp[N];
        azimuth[N];
        elevation[N];
    } // NOTA 1: potser hi ha altres maneres de fer-ho més òptim però és perquè ens entenguem
      // NOTA 2: pels moments inicials (sense captura) es podria indicar a l'antena que es situi al lloc per on sortirà el SV, més que no pas esperar-se a 0-0
    */

    // 2) Calibrar l'antena perquè estigui mirant a azimuth 0º i elevació 0º

    // 3) Per cada timeStep (1 segon, 0.5 segons...) moure els motors 

    // 4) Finalitzar el programa quan el SV ja no sigui visible.


    // NOTA 1: Cal veure si per fer la captura amb la SDR (USRP) cal executar cap altre codi (si es en paral·lel es podria fer servir MPI o OpenMP)
    // NOTA 2: Fer un sistema de referència de forma que el rang de moviment de l'antena sigui -360 ←→ +360, per evitar que els cables s'enrrollin entre ells 
    return 0;
}