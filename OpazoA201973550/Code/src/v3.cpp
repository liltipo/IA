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

vector<Ruta> generarSolucionGreedy(const InstanciaVRPB& instancia) {
    vector<Ruta> rutas;
    vector<bool> visitado(instancia.nodos.size(), false);
    visitado[0] = true; // depósito

    // Separar los clientes linehaul y backhaul por índices
    vector<int> pendientesLinehaul, pendientesBackhaul;
    for (size_t i = 1; i < instancia.nodos.size(); ++i) {
        if (instancia.nodos[i].tipo == 1) pendientesLinehaul.push_back(i);
        else if (instancia.nodos[i].tipo == 2) pendientesBackhaul.push_back(i);
    }

    for (int k = 0; k < instancia.cantidadVehiculos; ++k) {
        double carga = 0.0;
        Ruta ruta;
        ruta.nodos.push_back(1); // empezar en el depósito
        int actual = 0; // índice del depósito

        // Agregar LINEHAULS más cercanos que quepan
        while (true) {
            int mejor = -1;
            double mejorDist = numeric_limits<double>::max();

            for (int idx : pendientesLinehaul) {
                if (visitado[idx]) continue;
                double d = distanciaEuclidiana(instancia.nodos[actual], instancia.nodos[idx]);
                if (carga + instancia.nodos[idx].demanda <= instancia.capacidadVehiculo && d < mejorDist) {
                    mejor = idx;
                    mejorDist = d;
                }
            }

            if (mejor == -1) break;
            ruta.nodos.push_back(instancia.nodos[mejor].id);
            carga += instancia.nodos[mejor].demanda;
            visitado[mejor] = true;
            actual = mejor;
        }

        // Agregar BACKHAULS más cercanos que quepan
        while (true) {
            int mejor = -1;
            double mejorDist = numeric_limits<double>::max();

            for (int idx : pendientesBackhaul) {
                if (visitado[idx]) continue;
                double d = distanciaEuclidiana(instancia.nodos[actual], instancia.nodos[idx]);
                if (carga + instancia.nodos[idx].demanda <= instancia.capacidadVehiculo && d < mejorDist) {
                    mejor = idx;
                    mejorDist = d;
                }
            }

            if (mejor == -1) break;
            ruta.nodos.push_back(instancia.nodos[mejor].id);
            carga += instancia.nodos[mejor].demanda;
            visitado[mejor] = true;
            actual = mejor;
        }

        ruta.nodos.push_back(1); // volver al depósito
        ruta.demandaTotal = carga;
        rutas.push_back(ruta);
    }

    return rutas;
}

// ------------------ ARREGLAR SOLUCIONES NO FACTIBLES ------------------

void balancearSolucion(vector<Ruta>& rutas, const InstanciaVRPB& instancia, vector<bool>& visitado) {
    for (size_t idx = 1; idx < instancia.nodos.size(); ++idx) {
        if (visitado[idx]) continue;
        const Nodo& nodo = instancia.nodos[idx];

        bool insertado = false;

        // Intentamos insertar en la mejor posición posible
        for (Ruta& ruta : rutas) {
            // Verificamos capacidad
            if (ruta.demandaTotal + nodo.demanda > instancia.capacidadVehiculo) continue;

            // Buscamos posición válida:
            if (nodo.tipo == 1) {
                // LINEHAUL: solo se puede insertar antes del primer backhaul
                int insertPos = 1;
                for (size_t i = 1; i < ruta.nodos.size() - 1; ++i) {
                    int id = ruta.nodos[i];
                    if (instancia.nodos[id - 1].tipo == 2) break;
                    insertPos = i + 1;
                }

                ruta.nodos.insert(ruta.nodos.begin() + insertPos, nodo.id);
                ruta.demandaTotal += nodo.demanda;
                visitado[idx] = true;
                insertado = true;
                break;

            } else if (nodo.tipo == 2) {
                // BACKHAUL: insertar después del último linehaul pero antes del depósito
                int insertPos = ruta.nodos.size() - 1;
                for (int i = ruta.nodos.size() - 2; i >= 1; --i) {
                    int id = ruta.nodos[i];
                    if (instancia.nodos[id - 1].tipo == 1) {
                        insertPos = i + 1;
                        break;
                    }
                }

                ruta.nodos.insert(ruta.nodos.begin() + insertPos, nodo.id);
                ruta.demandaTotal += nodo.demanda;
                visitado[idx] = true;
                insertado = true;
                break;
            }
        }

        if (!insertado) {
            // Si no se pudo insertar directamente, buscar rutas para liberar espacio
            for (size_t i = 0; i < rutas.size(); ++i) {
                for (size_t j = 1; j < rutas[i].nodos.size() - 1; ++j) {
                    int movId = rutas[i].nodos[j];
                    if (instancia.nodos[movId - 1].tipo != nodo.tipo) continue; // solo mover del mismo tipo

                    double cargaMovida = instancia.nodos[movId - 1].demanda;
                    if (cargaMovida < nodo.demanda) continue;

                    for (size_t k = 0; k < rutas.size(); ++k) {
                        if (i == k) continue;
                        if (rutas[k].demandaTotal + cargaMovida <= instancia.capacidadVehiculo) {
                            // mover movId de i a k
                            rutas[k].nodos.insert(rutas[k].nodos.end() - 1, movId);
                            rutas[k].demandaTotal += cargaMovida;

                            rutas[i].nodos.erase(rutas[i].nodos.begin() + j);
                            rutas[i].demandaTotal -= cargaMovida;

                            // ahora insertar nodo original
                            rutas[i].nodos.insert(rutas[i].nodos.end() - 1, nodo.id);
                            rutas[i].demandaTotal += nodo.demanda;
                            visitado[idx] = true;
                            insertado = true;
                            break;
                        }
                    }
                    if (insertado) break;
                }
                if (insertado) break;
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

        // Intento de balancear clientes no visitados
        vector<bool> visitado(instancia.nodos.size(), false);
        visitado[0] = true;
        for (const Ruta& r : rutas)
            for (size_t i = 1; i < r.nodos.size() - 1; ++i)
                visitado[r.nodos[i] - 1] = true;

        balancearSolucion(rutas, instancia, visitado);

        // Evaluar la solución inicial corregida
        SolucionVRPB solucion = evaluarSolucion(rutas, instancia.nodos);

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
