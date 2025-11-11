#!/bin/bash

# Configuração
NS3_SCRIPT="equipe_1_2s2025"

# Pasta de base para todos os resultados
BASE_RESULT_DIR="resultados"
CSV_FILE="$BASE_RESULT_DIR/resultados_finais.csv"

# --- Listas de Parâmetros ---
CLIENTS=(1 2 4 8)
PROTOCOLS=("TCP" "UDP" "Mixed")
MOBILITY=("false" "true")

# --- Início do Script ---
echo "Iniciando script de simulação..."

# Compila o projeto ns-3
./ns3 build
if [ $? -ne 0 ]; then
    echo "Erro na compilação! Abortando."
    exit 1
fi

mkdir -p $BASE_RESULT_DIR
echo "Salvando resultados em: $BASE_RESULT_DIR"
rm -f *.pcap

#Loop Principal
for proto in "${PROTOCOLS[@]}"; do
    for mobile in "${MOBILITY[@]}"; do
        for clients in "${CLIENTS[@]}"; do
            
            if [ "$mobile" == "true" ]; then
                MOB_LABEL="Movel"
            else
                MOB_LABEL="Estatico"
            fi

            echo "------------------------------------------------"
            echo "Executando: $proto, $MOB_LABEL, $clients Clientes"
            echo "------------------------------------------------"

            DIR_NAME="${proto}_${MOB_LABEL}_${clients}_Clientes"
            FULL_PATH="$BASE_RESULT_DIR/$DIR_NAME"
            mkdir -p $FULL_PATH

            OUTPUT_FILE="$FULL_PATH/output.txt"
            PCAP_PREFIX="$FULL_PATH/"

            ./ns3 run "scratch/$NS3_SCRIPT" -- --protocolo=$proto --nSta=$clients --cenarioMovel=$mobile --pcapPrefix=$PCAP_PREFIX &> $OUTPUT_FILE
            
        done
    done
done

echo "------------------------------------------------"
echo "Todas as simulações foram concluídas!"
echo "------------------------------------------------"

# Seção de Coleta de Dados para CSV
echo "Coletando resultados para $CSV_FILE..."

# 1. Cria o arquivo CSV e adiciona o cabeçalho
echo "Protocolo,Clientes,Mobilidade,Vazao_Mbps,Atraso_ms,Perda_pct" > $CSV_FILE

# 2. Encontra todos os 'output.txt', "pesca" a linha "ResumoCSV" e a formata
for f in $(find $BASE_RESULT_DIR -name "output.txt"); do
    # grep: encontra a linha | awk: remove o "ResumoCSV," e imprime o resto
    grep "ResumoCSV" "$f" | awk -F ',' '{print $2","$3","$4","$5","$6","$7}' >> $CSV_FILE
done

echo "Coleta de dados para CSV concluída!"
echo "Verifique '$BASE_RESULT_DIR' e '$CSV_FILE'."
# --- FIM DA SEÇÃO NOVA ---