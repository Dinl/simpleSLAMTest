// SANN.cpp: define el punto de entrada de la aplicaci�n de consola.
//
#include "stdafx.h"
#include "SANN.h"

SANN::~SANN(){

}

/*********************************************************************************************
*	Funcion privada de la clase de clasificacion SANN
*	Descriptors1 -> Matriz original con los descriptores de esntrenamiento
*	Descriptors2 -> Matriz original con los descriptores de clasificacion
*	Matches		 -> Indices Resultado de match
*
*	TODO: 
*		  Agregar el rango dinamico
*		  Hacer que el coeficiente sea adaptativo
*********************************************************************************************/
void SANN::Match(cv::Mat &Descriptors1, cv::Mat &Descriptors2, std::vector<cv::DMatch> &Matches){
	//Primero, entrenar con el primer argumento
	if(Descriptors1.rows > 0 && Descriptors1.cols > 0)
		train(Descriptors1);
	else
		std::cout << "No se puede entrenar, la matriz 1 no contiene muestras o caracteristicas \n";

	if((Descriptors2.rows > 0 && Descriptors2.cols > 0))
		Descriptors2.copyTo(Descriptores2);
	else
		std::cout << "No se puede clasificar, la matriz 2 no contiene muestras o caracteristicas \n";

	if(Descriptors1.cols != Descriptors2.cols)
		std::cout << "No se puede clasificar, la matriz 1 y la matriz 2 no tienen el mismo numero de caracteristicas \n";

	muestrasClasificacion = Descriptors2.rows;
	
	//Distribuir aleatoriamente las muestras de clasificacion
	randomDistribution(muestrasEntrenamiento, muestrasClasificacion);

	//Calcular las distancias
	for(int i=0; i < muestrasEntrenamiento; i++){
		int indexN = Material.at<float>(i,0);
		int indexM = Material.at<float>(i,1);
		if(indexM != -1)
			Material.at<float>(i,3) = distance(indexN, indexM);
	}

	//Proponer vector de cambio
	for(int i=0; i<100; i++){
		float coef = std::exp(-i*coeficiente);
		proposeRandomPair(coef);
	}

	//Llenar la matriz de Match
	for(int i=0; i < muestrasEntrenamiento; i++)
		Matches.push_back(cv::DMatch(Material.at<float>(i,0), Material.at<float>(i,1), Material.at<float>(i,2), Material.at<float>(i,3)));

}

/*********************************************************************************************
*	Funcion privada de la clase de clasificacion SANN
*	Descriptors -> Matriz original con los descriptores adentro
*
*	TODO: Agregar una columna de indice cuando se va a entrenar para pasarla al material
*			y no perder la referencia original
*********************************************************************************************/
void SANN::train(cv::Mat Descriptors){
	//Hallar el numero de muestras y el numero de caracteristicas
	muestrasEntrenamiento = Descriptors.rows;
	caracteristicas = Descriptors.cols;

	//Declarar las matrices de Sumas y desviaciones
	Sumas = cv::Mat(1,caracteristicas,CV_32FC1,cv::Scalar(0));
	Medias = cv::Mat(1,caracteristicas,CV_32FC1,cv::Scalar(0));
	Desviaciones = cv::Mat(1,caracteristicas,CV_32FC1,cv::Scalar(0));

	//Hallas las sumas
	for(int i=0; i<muestrasEntrenamiento; i++)
		for(int j=0; j<caracteristicas; j++)
			Sumas.at<float>(0,j) =  Sumas.at<float>(0,j) + Descriptors.at<float>(i,j);

	//Hallar las medias
	for(int j=0; j<caracteristicas; j++)
			Medias.at<float>(0,j) = Sumas.at<float>(0,j) / muestrasEntrenamiento;

	//Hallar las desviaciones estandar
	for(int i=0; i<muestrasEntrenamiento; i++)
		for(int j=0; j<caracteristicas; j++)
			Desviaciones.at<float>(0,j) =  Desviaciones.at<float>(0,j)  + cv::pow(Descriptors.at<float>(i,j) - Medias.at<float>(0,j), 2);
	
	for(int j=0; j<caracteristicas; j++)
		Desviaciones.at<float>(0,j) = cv::sqrt(Desviaciones.at<float>(0,j) / muestrasEntrenamiento);

	//Hallar la columna con mayor desviacion estandar
	int maxDesv = -9999999999, maxDesvIdx = 0;
	for(int i=0; i<caracteristicas; i++)
		if(Desviaciones.at<float>(0,i) > maxDesv){
			maxDesv = Desviaciones.at<float>(0,i);
			maxDesvIdx = i;
		}
	
	//Crear el material, para optimizar el proceso es una matriz con N filas como Descriptores de entrenamiento
	//y 4 columnas:
	//1) Indice de fila de la muestra material de entrenamiento
	//2) Indice de fila de la muestra a clasificar (Inicialmente aleatorio por principio de diversidad), -1 defecto
	//3) Indice propuesto de cambio, -1 por defecto
	//4) Distancia entre las muestras
	Material = cv::Mat::zeros(muestrasEntrenamiento, 4, CV_32F);

	for(int i=0; i < muestrasEntrenamiento; i++){
		Material.at<float>(i,0) = i;
		Material.at<float>(i,1) = -1;
		Material.at<float>(i,2) = -1;
		Material.at<float>(i,3) = 150000;
	}

	//Finalmente se ordenan los descriptores por la columna con mayor desviacion
	sortByCol(Descriptors, Descriptores1, maxDesvIdx);
}

/*********************************************************************************************
*	Funcion privada de la clase de clasificacion SANN
*	src -> Matriz original con las muestras desordenadas
*	dst -> Matriz resultado ordenada
*	col -> Indice de la columna que se quiere ordenar
*	
*	TODO: El indice original esta en 2, cambiar a 0, y ajustar todas las funciones
*********************************************************************************************/
void SANN::sortByCol(cv::Mat &src, cv::Mat &dst, int col){
	//Primero hallar el orden de los indices de cada columna
	cv::Mat idx;
	cv::sortIdx(src, idx, CV_SORT_EVERY_COLUMN + CV_SORT_ASCENDING);

	//Segundo se crea la matriz de destino igual a la matriz fuente
	cv::Mat sorted = cv::Mat::zeros(src.rows,src.cols,src.type());

	//Se itera y se copia fila por fila
	for(int i=0; i<sorted.rows; i++){
		src.row(idx.at<int>(i,col)).copyTo(sorted.row(i));
		Material.at<float>(i,2) = idx.at<int>(i,col);
	}

	//Se copia a la salida
	sorted.copyTo(dst);
}

/*********************************************************************************************
*	Funcion privada de la clase de clasificacion SANN
*	N -> Numero de  muestras de entrenamiento
*	M -> Numero de muestras a clasificar
*
*	!IMPORTANTE:  N => M
*********************************************************************************************/
void SANN::randomDistribution(int N, int M){
	//Crear el vector base y llenarlo
	std::vector<int> listaBaseEntrenamiento;
	for(int i=0; i < M; i++)
		listaBaseEntrenamiento.push_back(i);

	//Crear la lista desordenada y llenarla
	std::vector<int> listaBaseClasificacion;
	srand (time(NULL));
	int El = M;
	for(int i=0; i < M; i++){
		int index = rand() % El;
		listaBaseClasificacion.push_back(listaBaseEntrenamiento.at(index));
		listaBaseEntrenamiento.erase(listaBaseEntrenamiento.begin() + index);
		if(!--El)
			break;
	}

	//Crear la lista de pareja iniciarla en -1
	std::vector<int> listaBasePareja;
	for(int i=0; i < N; i++)
		listaBasePareja.push_back(-1);

	//Ubicar los indice de la lista de baseClasificacion en la de pareja
	El = M;
	for(int i=0; i < M; i++){
		while(true){
			int index = rand() % N;
			if(listaBasePareja.at(index) == -1){
				listaBasePareja.at(index) = listaBaseClasificacion.at(i);
				break;
			}
		}
	}

	//Finalmente llenar los indices en el Material
	for(int i=0; i < N; i++)
		Material.at<float>(i,1) = listaBasePareja.at(i);
}

/*********************************************************************************************
*	Funcion privada de la clase de clasificacion SANN
*	N -> Indice de la muestra de entrenamiento
*	M -> Indice de la muestra de clasificacion
*
*	IMPORTANTE: N,M deben ser valores existentes!
*	TODO: Optimizar con absdiff().sum()
*********************************************************************************************/
float SANN::distance(int N, int M){
	float d = 0;
	for(int i=0; i<caracteristicas; i++){
		float  resta = Descriptores1.at<float>(N,i) - Descriptores2.at<float>(M,i);
		d += std::abs(resta);
	}

	return d;
}

/*********************************************************************************************
*	Funcion privada de la clase de clasificacion SANN y metodo principal basado en SA
*
*	TODO: Optimizar con absdiff().sum()
*		Habilitar o deshabilitar debug con variable global
*********************************************************************************************/
void SANN::proposeRandomPair(float coeff){
	int Ncambios = 0;
	int NcambiosAleatorios = 0;
	//Recorrer el material en busca de particulas a clasificar
	srand (time(NULL));
	int El = muestrasEntrenamiento;
	for(int i=0; i < muestrasEntrenamiento; i++){
		int indexM = Material.at<float>(i,1);
		//Si encuentra una particula de clasificacion
		if(indexM != -1){
			//Buscar en el espacio de indices propuesto una posibilidad de cambio
			int indexA = rand() % El;
			//Se calcula la funcion de costo
			float d_proposed = distance(indexA,indexM);
			float d_actual = Material.at<float>(i,3);

			//Se calcula un numero aleatorio entre 0 y 1
			float aleatorio_unitario = (rand() % 100)/100.0;

			//Si se mejora la funcion de costo o la funcion de probabilidad la acepta
			if((d_proposed < d_actual || aleatorio_unitario < coeff) && i != indexA){
				//Para hacer debug, imprimir primero los descriptores propuestos
				//std::cout << "\n \n Material antes de: \n";
				//DEBUG(i,indexA,false);
				if(aleatorio_unitario < coeff) NcambiosAleatorios++;

				//Si el espacio esta vacio, entonces se pasa la particula a ese espacio
				if(Material.at<float>(indexA,1) == -1){
					Material.at<float>(indexA,1) = indexM;
					Material.at<float>(indexA,3) = d_proposed;
					Material.at<float>(i,1) = -1;
					Material.at<float>(i,3) = 150000;
					Ncambios++;
				}
				//Sino esta vacio, se verifica que la funcion de costo mejore respecto al valor actual
				else if(d_proposed < Material.at<float>(indexA,3) || aleatorio_unitario < coeff){
					Material.at<float>(i,1) = Material.at<float>(indexA,1);
					Material.at<float>(i,3) = distance(Material.at<float>(i,0), Material.at<float>(i,1));					
					Material.at<float>(indexA,1) = indexM;
					Material.at<float>(indexA,3) = d_proposed;
					Ncambios++;
				}

				//std::cout << "\n Material despues de: \n";
				//DEBUG(i,indexA,true);
			}
		}
	}
	//std::cout << Ncambios << " " << NcambiosAleatorios << " " << distanciaPromedio() <<"\n";
}

/*********************************************************************************************
*	Funcion privada de la clase de clasificacion SANN que imprime el material
*
*	TODO: Optimizar con absdiff().sum()
*********************************************************************************************/
void SANN::toString(){
	std::cout << "Material: \n \n";
	for(int i=0; i < muestrasEntrenamiento; i++){
		for(int j=0; j < 4; j++)
			std::cout << Material.at<float>(i,j) << " ";
		std::cout << "\n";
	}
}

/*********************************************************************************************
*	Funcion privada de la clase de clasificacion SANN que imprime el descriptor1 en la muestra
*	solicitada
*	
*	row	->	Fila solicitada
*
*********************************************************************************************/
void SANN::descriptor1AtRow(int row){
	if(row >= 0){
		for(int i=0; i<Descriptores1.cols; i++)
			std::cout << Descriptores1.at<float>(row,i) << " ";
		std::cout << "\n";
	}
}

/*********************************************************************************************
*	Funcion privada de la clase de clasificacion SANN que imprime el descriptor2 en la muestra
*	solicitada
*
*	row	->	Fila solicitada
*
*********************************************************************************************/
void SANN::descriptor2AtRow(int row){
	if(row >= 0){
		for(int i=0; i<Descriptores2.cols; i++)
			std::cout << Descriptores2.at<float>(row,i) << " ";
		std::cout << "\n";
	}
}

/*********************************************************************************************
*	Funcion privada de la clase de clasificacion SANN que imprime el rastro DEBUG de los cambios
*	realizados durante la clasifcacion
*
*	m1	->	Primer indice del material
*	m2	->	Segundo indice del material
*	print_descriptors	->	Si es true, se imprimen los descriptores de las filas
*
*********************************************************************************************/
void SANN::DEBUG(int m1, int m2, bool print_descriptors){

	std::cout << Material.at<float>(m1,0) << " "<< Material.at<float>(m1,1) << " "<< Material.at<float>(m1,3) <<"\n";
	std::cout << Material.at<float>(m2,0) << " "<< Material.at<float>(m2,1) << " "<< Material.at<float>(m2,3) <<"\n";

	if(print_descriptors){
		std::cout << "\n Descriptores usados: \n";
		descriptor1AtRow(Material.at<float>(m1,0));
		descriptor2AtRow(Material.at<float>(m1,1));
		descriptor1AtRow(Material.at<float>(m2,0));
		descriptor2AtRow(Material.at<float>(m2,1));
	}

}

/*********************************************************************************************
*	Funcion privada de la clase de clasificacion SANN que calcula la distancia promedio actual
*	de las particulas con pareja
*
*********************************************************************************************/
float SANN::distanciaPromedio(){
	float total = 0;
	
	for(int i=0; i < muestrasEntrenamiento; i++)
		if(Material.at<float>(i,3) != 150000)
			total += Material.at<float>(i,3);

	return total/muestrasEntrenamiento;
}

void SANN::setCoefficiente(float coeff){
	coeficiente = coeff;
}