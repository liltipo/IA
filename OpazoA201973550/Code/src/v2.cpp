#include <iostream>
#include <stdio.h>
#include <fstream>
#include <vector>
#include <sstream>
#include <string>
#include <stdexcept>
#include <cmath>
#include <chrono>
#include <limits>
#include <algorithm>

using namespace std;

// ------------------ ESTRUCTURAS ------------------

struct Nodo {
    int tipo; // 0: depósito, 1: linehaul, 2: backhaul
    int id;
    double x, y;
    double demanda = 0.0;
};

struct Ruta {
    vector<int> nodos;
    double distancia = 0.0;
    double demandaTotal = 0.0;
};

struct SolucionVRPB {
    vector<Ruta> rutas;
    double valorFuncion = 0.0;
};

struct InstanciaVRPB {
    int cantidadNodos;
    vector<Nodo> nodos;
    int cantidadVehiculos;
    double capacidadVehiculo;
};

// ------------------ PARSER ------------------

InstanciaVRPB leerInstancia(const string& nombreArchivo) {
    InstanciaVRPB instancia;
    ifstream archivo(nombreArchivo);
    if (!archivo) throw runtime_error("No se pudo abrir el archivo");

    archivo >> instancia.cantidadNodos;
    for (int i = 0; i < instancia.cantidadNodos; ++i) {
        Nodo nodo;
        archivo >> nodo.tipo >> nodo.id >> nodo.x >> nodo.y;
        instancia.nodos.push_back(nodo);
    }

    archivo >> instancia.cantidadVehiculos >> instancia.capacidadVehiculo;

    // Nos saltamos el depósito (nodo 0)
    for (int i = 0; i < instancia.cantidadNodos - 1; ++i) {
        int id;
        double demanda;
        archivo >> id >> demanda;
        for (auto& nodo : instancia.nodos) {
            if (nodo.id == id) {
                nodo.demanda = demanda;
                break;
            }
        }
    }

    return instancia;
}

// ------------------ FUNCIONES DE EVALUACIÓN ------------------

double distanciaEuclidiana(const Nodo& a, const Nodo& b) {
    double dx = a.x - b.x;
    double dy = a.y - b.y;
    return sqrt(dx * dx + dy * dy);
}

double evaluarRuta(const Ruta& ruta, const vector<Nodo>& nodos) {
    double total = 0.0;
    for (size_t i = 0; i + 1 < ruta.nodos.size(); ++i) {
        const Nodo& a = nodos[ruta.nodos[i] - 1];
        const Nodo& b = nodos[ruta.nodos[i + 1] - 1];
        total += distanciaEuclidiana(a, b);
    }
    return total;
}

SolucionVRPB evaluarSolucion(vector<Ruta>& rutas, const vector<Nodo>& nodos) {
    SolucionVRPB solucion;
    solucion.rutas = rutas;
    solucion.valorFuncion = 0.0;

    for (Ruta& ruta : solucion.rutas) {
        ruta.distancia = evaluarRuta(ruta, nodos);
        ruta.demandaTotal = 0.0;
        for (int id : ruta.nodos)
            ruta.demandaTotal += nodos[id - 1].demanda;

        solucion.valorFuncion += ruta.distancia;
    }

    return solucion;
}

// ------------------ VALIDACIÓN ------------------

bool esRutaFactible(const Ruta& ruta, const vector<Nodo>& nodos, double capacidad) {
    double carga = 0.0;
    bool faseBackhaul = false;
    for (size_t i = 1; i < ruta.nodos.size() - 1; ++i) {
        int id = ruta.nodos[i];
        const Nodo& nodo = nodos[id - 1];

        if (nodo.tipo == 1) {
            if (faseBackhaul) return false;
            carga += nodo.demanda;
        } else if (nodo.tipo == 2) {
            faseBackhaul = true;
            carga += nodo.demanda;
        }

        if (carga > capacidad) return false;
    }
    return true;
}

bool esSolucionFactible(const vector<Ruta>& rutas, const vector<Nodo>& nodos, double capacidad) {
    vector<bool> visitado(nodos.size(), false);
    for (const Ruta& ruta : rutas) {
        if (!esRutaFactible(ruta, nodos, capacidad)) return false;
        for (size_t i = 1; i < ruta.nodos.size() - 1; ++i) {
            int id = ruta.nodos[i];
            if (visitado[id - 1]) return false;
            visitado[id - 1] = true;
        }
    }

    // Todos los clientes (excepto el depósito) deben ser visitados una vez
    int clientesEsperados = nodos.size() - 1; // Todos excepto depósito
    int clientesVisitados = 0;
    for (size_t i = 1; i < visitado.size(); ++i)
        if (visitado[i]) ++clientesVisitados;

    if (clientesVisitados != clientesEsperados)
        return false;

    return true;
}

// ------------------ ESCRITURA ------------------

void escribirSalida(const string& archivoSalida, const SolucionVRPB& solucion, double tiempoSegundos){
    ofstream salida(archivoSalida);

    int totalClientes = 0;
    for (const auto& ruta : solucion.rutas)
        totalClientes += ruta.nodos.size() - 2;

    salida << solucion.valorFuncion << " "
           << totalClientes << " "
           << solucion.rutas.size() << " "
           << tiempoSegundos << "\n";

    for (const auto& ruta : solucion.rutas) {
        for (int id : ruta.nodos)
            salida << id << " ";
        salida << ruta.distancia << " " << ruta.demandaTotal << "\n";
    }
    salida.close();
}

// ----------------------------------------------
// HEURISTICA GREEDY VRPB
// ----------------------------------------------

// ------------------ HEURISTICA GREEDY MEJORADA (CONSTRUCCIÓN PARALELA) ------------------
vector<Ruta> generarSolucionGreedy(const InstanciaVRPB& instancia) {
    vector<Ruta> rutas(instancia.cantidadVehiculos);
    vector<int> posActual(rutas.size(), 0); // Índices del último nodo en cada ruta
    vector<bool> visitado(instancia.nodos.size(), false);
    visitado[0] = true; // Depósito

    // Inicializar rutas con depósito
    for (int k = 0; k < instancia.cantidadVehiculos; ++k) {
        rutas[k].nodos.push_back(1);
    }

    // Separar pendientes
    vector<int> pendientesLinehaul, pendientesBackhaul;
    for (size_t i = 1; i < instancia.nodos.size(); ++i) {
        if (instancia.nodos[i].tipo == 1) pendientesLinehaul.push_back(i);
        else if (instancia.nodos[i].tipo == 2) pendientesBackhaul.push_back(i);
    }

    // Asignación paralela de linehauls
    bool asignado = true;
    while (!pendientesLinehaul.empty() && asignado) {
        asignado = false;
        for (int k = 0; k < rutas.size(); ++k) {
            if (pendientesLinehaul.empty()) break;
            Ruta& ruta = rutas[k];
            int actual = posActual[k];
            double carga = ruta.demandaTotal;

            int mejorIdx = -1;
            double mejorDist = numeric_limits<double>::max();
            int mejorPos = -1;

            // Buscar mejor linehaul factible
            for (int j = 0; j < pendientesLinehaul.size(); ++j) {
                int idx = pendientesLinehaul[j];
                const Nodo& cand = instancia.nodos[idx];
                double dist = distanciaEuclidiana(instancia.nodos[actual], cand);
                if (!visitado[idx] && carga + cand.demanda <= instancia.capacidadVehiculo && dist < mejorDist) {
                    mejorIdx = idx;
                    mejorDist = dist;
                    mejorPos = j;
                }
            }

            // Insertar en ruta si se encontró
            if (mejorIdx != -1) {
                ruta.nodos.push_back(instancia.nodos[mejorIdx].id);
                ruta.demandaTotal += instancia.nodos[mejorIdx].demanda;
                visitado[mejorIdx] = true;
                posActual[k] = mejorIdx;
                pendientesLinehaul.erase(pendientesLinehaul.begin() + mejorPos);
                asignado = true;
            }
        }
    }

    // Asignación paralela de backhauls (misma lógica)
    asignado = true;
    while (!pendientesBackhaul.empty() && asignado) {
        asignado = false;
        for (int k = 0; k < rutas.size(); ++k) {
            if (pendientesBackhaul.empty()) break;
            Ruta& ruta = rutas[k];
            int actual = posActual[k];
            double carga = ruta.demandaTotal;

            int mejorIdx = -1;
            double mejorDist = numeric_limits<double>::max();
            int mejorPos = -1;

            for (int j = 0; j < pendientesBackhaul.size(); ++j) {
                int idx = pendientesBackhaul[j];
                const Nodo& cand = instancia.nodos[idx];
                double dist = distanciaEuclidiana(instancia.nodos[actual], cand);
                if (!visitado[idx] && carga + cand.demanda <= instancia.capacidadVehiculo && dist < mejorDist) {
                    mejorIdx = idx;
                    mejorDist = dist;
                    mejorPos = j;
                }
            }

            if (mejorIdx != -1) {
                ruta.nodos.push_back(instancia.nodos[mejorIdx].id);
                ruta.demandaTotal += instancia.nodos[mejorIdx].demanda;
                visitado[mejorIdx] = true;
                posActual[k] = mejorIdx;
                pendientesBackhaul.erase(pendientesBackhaul.begin() + mejorPos);
                asignado = true;
            }
        }
    }

    // Cerrar rutas con depósito
    for (Ruta& ruta : rutas) {
        ruta.nodos.push_back(1);
    }

    return rutas;
}

// ------------------ FUNCIÓN DE REPARACIÓN ------------------
bool intentarInsertarEnRuta(Ruta& ruta, const Nodo& cliente, double capacidad, const vector<Nodo>& todosNodos) {
    // Verificar capacidad
    if (ruta.demandaTotal + cliente.demanda > capacidad) {
        return false;
    }

    // Encontrar posición válida según tipo de cliente
    vector<size_t> posicionesValidas;
    if (cliente.tipo == 1) { // Linehaul: antes de backhauls
        size_t primerBackhaul = ruta.nodos.size();
        for (size_t i = 1; i < ruta.nodos.size() - 1; ++i) {
            if (todosNodos[ruta.nodos[i] - 1].tipo == 2) {
                primerBackhaul = i;
                break;
            }
        }
        for (size_t i = 1; i < primerBackhaul; ++i) {
            posicionesValidas.push_back(i);
        }
    } else { // Backhaul: después de linehauls
        size_t ultimoLinehaul = 0;
        for (size_t i = ruta.nodos.size() - 1; i > 0; --i) {
            if (todosNodos[ruta.nodos[i] - 1].tipo == 1) {
                ultimoLinehaul = i;
                break;
            }
        }
        for (size_t i = ultimoLinehaul + 1; i < ruta.nodos.size(); ++i) {
            posicionesValidas.push_back(i);
        }
    }

    // Buscar mejor posición de inserción
    double mejorCosto = numeric_limits<double>::max();
    size_t mejorPos = 0;
    for (size_t pos : posicionesValidas) {
        vector<int> copia = ruta.nodos;
        copia.insert(copia.begin() + pos, cliente.id);
        
        Ruta rutaTemporal;
        rutaTemporal.nodos = copia;
        double costo = evaluarRuta(rutaTemporal, todosNodos); // <-- Corrección aquí
        
        if (costo < mejorCosto) {
            mejorCosto = costo;
            mejorPos = pos;
        }
    }

    // Insertar si se encontró posición válida
    if (!posicionesValidas.empty()) {
        ruta.nodos.insert(ruta.nodos.begin() + mejorPos, cliente.id);
        ruta.demandaTotal += cliente.demanda;
        return true;
    }
    return false;
}

bool redistribuirParaInsertar(Ruta& ruta1, Ruta& ruta2, const Nodo& cliente, 
                            double capacidad, const vector<Nodo>& todosNodos) {
    // 1. Primero intentar mover clientes pequeños de ruta1 a ruta2
    for (size_t i = 1; i < ruta1.nodos.size() - 1; ++i) {
        const Nodo& candidato = todosNodos[ruta1.nodos[i] - 1];
        if (ruta2.demandaTotal + candidato.demanda <= capacidad) {
            // Mover el cliente
            int id = ruta1.nodos[i];
            ruta1.nodos.erase(ruta1.nodos.begin() + i);
            ruta1.demandaTotal -= candidato.demanda;
            
            // Intentar insertar en ruta2 manteniendo factibilidad
            Ruta copiaRuta2 = ruta2;
            if (intentarInsertarEnRuta(copiaRuta2, candidato, capacidad, todosNodos)) {
                // Si se puede insertar en ruta2, intentar insertar el cliente original en ruta1
                Ruta copiaRuta1 = ruta1;
                if (intentarInsertarEnRuta(copiaRuta1, cliente, capacidad, todosNodos)) {
                    // Aplicar los cambios definitivos
                    ruta2 = copiaRuta2;
                    ruta1 = copiaRuta1;
                    return true;
                }
            }
            
            // Revertir si falla
            ruta1.nodos.insert(ruta1.nodos.begin() + i, id);
            ruta1.demandaTotal += candidato.demanda;
        }
    }
    
    // 2. Intentar el proceso inverso (mover de ruta2 a ruta1)
    for (size_t i = 1; i < ruta2.nodos.size() - 1; ++i) {
        const Nodo& candidato = todosNodos[ruta2.nodos[i] - 1];
        if (ruta1.demandaTotal + candidato.demanda <= capacidad) {
            // Mover el cliente
            int id = ruta2.nodos[i];
            ruta2.nodos.erase(ruta2.nodos.begin() + i);
            ruta2.demandaTotal -= candidato.demanda;
            
            // Intentar insertar en ruta1
            Ruta copiaRuta1 = ruta1;
            if (intentarInsertarEnRuta(copiaRuta1, candidato, capacidad, todosNodos)) {
                // Intentar insertar el cliente original en ruta2
                Ruta copiaRuta2 = ruta2;
                if (intentarInsertarEnRuta(copiaRuta2, cliente, capacidad, todosNodos)) {
                    // Aplicar los cambios
                    ruta1 = copiaRuta1;
                    ruta2 = copiaRuta2;
                    return true;
                }
            }
            
            // Revertir si falla
            ruta2.nodos.insert(ruta2.nodos.begin() + i, id);
            ruta2.demandaTotal += candidato.demanda;
        }
    }
    
    return false;
}

void repararSolucionMejorada(vector<Ruta>& rutas, vector<int>& noAsignados, 
                           double capacidad, const vector<Nodo>& todosNodos) {
    // Fase 1: Intento directo de inserción
    for (auto it = noAsignados.begin(); it != noAsignados.end(); ) {
        const Nodo& cliente = todosNodos[*it];
        bool inserted = false;
        
        // Ordenar rutas por capacidad residual descendente
        sort(rutas.begin(), rutas.end(), 
             [capacidad](const Ruta& a, const Ruta& b) {
                 return (capacidad - a.demandaTotal) > (capacidad - b.demandaTotal);
             });

        for (Ruta& ruta : rutas) {
            if (intentarInsertarEnRuta(ruta, cliente, capacidad, todosNodos)) {
                it = noAsignados.erase(it);
                inserted = true;
                break;
            }
        }
        if (!inserted) ++it;
    }

    // Fase 2: Redistribución agresiva si aún hay no asignados
    while (!noAsignados.empty()) {
        bool progreso = false;
        
        for (auto it = noAsignados.begin(); it != noAsignados.end(); ) {
            const Nodo& cliente = todosNodos[*it];
            
            // Buscar combinación de 2 rutas donde podamos redistribuir
            for (size_t i = 0; i < rutas.size(); ++i) {
                for (size_t j = i+1; j < rutas.size(); ++j) {
                    if (redistribuirParaInsertar(rutas[i], rutas[j], cliente, capacidad, todosNodos)) {
                        it = noAsignados.erase(it);
                        progreso = true;
                        goto next_cliente;
                    }
                }
            }
            ++it;
            next_cliente:;
        }

        if (!progreso) {
            // Estrategia de último recurso: forzar inserción en la ruta con más espacio
            for (auto it = noAsignados.begin(); it != noAsignados.end(); ) {
                const Nodo& cliente = todosNodos[*it];
                auto mejorRuta = min_element(rutas.begin(), rutas.end(),
                    [capacidad](const Ruta& a, const Ruta& b) {
                        return (capacidad - a.demandaTotal) > (capacidad - b.demandaTotal);
                    });
                
                // Forzar inserción al final (antes del depósito)
                mejorRuta->nodos.insert(mejorRuta->nodos.end() - 1, cliente.id);
                mejorRuta->demandaTotal += cliente.demanda;
                it = noAsignados.erase(it);
            }
        }
    }
}

// ----------------------------------------------
// HILL CLIMBING - BEST IMPROVEMENT
// ----------------------------------------------

SolucionVRPB hillClimbingBI(const SolucionVRPB& solucionInicial, const InstanciaVRPB& instancia) {
    SolucionVRPB actual = solucionInicial;
    bool mejora = true;

    while (mejora) {
        mejora = false;
        SolucionVRPB mejorVecino = actual;
        double mejorCosto = actual.valorFuncion;

        for (size_t i = 0; i < actual.rutas.size(); ++i) {
            for (size_t j = 1; j + 1 < actual.rutas[i].nodos.size(); ++j) {
                int cliente = actual.rutas[i].nodos[j];
                if (cliente == instancia.nodos[0].id) continue;

                for (size_t k = 0; k < actual.rutas.size(); ++k) {
                    if (i == k) continue;

                    for (size_t pos = 1; pos < actual.rutas[k].nodos.size(); ++pos) {
                        auto copia = actual.rutas;

                        // remover cliente de i
                        copia[i].nodos.erase(copia[i].nodos.begin() + j);

                        // insertar en k en pos
                        copia[k].nodos.insert(copia[k].nodos.begin() + pos, cliente);

                        if (!esSolucionFactible(copia, instancia.nodos, instancia.capacidadVehiculo))
                            continue;

                        SolucionVRPB candidato = evaluarSolucion(copia, instancia.nodos);
                        if (candidato.valorFuncion < mejorCosto) {
                            mejorCosto = candidato.valorFuncion;
                            mejorVecino = candidato;
                            mejora = true;
                        }
                    }
                }
            }
        }

        if (mejora)
            actual = mejorVecino;
    }

    return actual;
}

// ------------------ MAIN ------------------

int main(int argc, char* argv[]) {

    string archivo = argv[1];
    auto inicio = chrono::high_resolution_clock::now();

    try {
        InstanciaVRPB instancia = leerInstancia(archivo);
        if (argc < 3) {
            cerr << "Uso: " << argv[0] << " <archivo_instancia> <archivo_salida>" << endl;
            return 1;
        }

        string archivo = argv[1];
        string archivoSalida = argv[2];

        // SOLUCIÓN INICIAL GREEDY
        vector<Ruta> rutas = generarSolucionGreedy(instancia);
        SolucionVRPB solucion = evaluarSolucion(rutas, instancia.nodos);

        // Identificar clientes no asignados
        vector<bool> visitado(instancia.nodos.size(), false);
        visitado[0] = true; // Depósito
        for (const Ruta& ruta : rutas) {
            for (size_t i = 1; i < ruta.nodos.size() - 1; ++i) {
                visitado[ruta.nodos[i] - 1] = true;
            }
        }
        vector<int> noAsignados;
        for (size_t i = 1; i < instancia.nodos.size(); ++i) {
            if (!visitado[i]) noAsignados.push_back(i);
        }

        // Reparar solución si hay no asignados
        if (!noAsignados.empty()) {
            repararSolucionMejorada(rutas, noAsignados, instancia.capacidadVehiculo, instancia.nodos);
            solucion = evaluarSolucion(rutas, instancia.nodos);
        }

        // MEJORA POR HILL CLIMBING BEST IMPROVEMENT
        solucion = hillClimbingBI(solucion, instancia);

        if (!esSolucionFactible(solucion.rutas, instancia.nodos, instancia.capacidadVehiculo)) {
            cerr << "ADVERTENCIA: solución " << archivo << " no factible.\n";
        }

        auto fin = chrono::high_resolution_clock::now();
        chrono::duration<double> duracion = fin - inicio;

        escribirSalida(archivoSalida, solucion, duracion.count());
        cout << "Instancia procesada correctamente. Archivo generado: " << archivoSalida << "\n";

    } catch (const exception& e) {
        cerr << "Error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
