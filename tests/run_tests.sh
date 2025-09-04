#!/bin/bash

# Script para executar todos os testes do projeto IPC
# Uso: ./run_tests.sh [unit|integration|all]

set -e

# Cores para output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Diretórios
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="$SCRIPT_DIR/build"

# Função para imprimir mensagens coloridas
print_info() {
    echo -e "${YELLOW}[INFO]${NC} $1"
}

print_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

print_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# Função para configurar e compilar testes
build_tests() {
    print_info "Configurando ambiente de testes..."
    
    # Cria diretório de build
    mkdir -p "$BUILD_DIR"
    cd "$BUILD_DIR"
    
    # Configura CMake
    print_info "Executando cmake..."
    cmake .. -DCMAKE_BUILD_TYPE=Debug
    
    # Compila
    print_info "Compilando testes..."
    make -j$(nproc)
    
    print_success "Compilação concluída!"
}

# Função para executar testes unitários
run_unit_tests() {
    print_info "Executando testes unitários..."
    
    if [ -f "$BUILD_DIR/unit_tests" ]; then
        cd "$BUILD_DIR"
        ./unit_tests --gtest_color=yes --gtest_output=xml:unit_test_results.xml
        print_success "Testes unitários concluídos!"
    else
        print_error "Executável de testes unitários não encontrado!"
        return 1
    fi
}

# Função para executar testes de integração
run_integration_tests() {
    print_info "Executando testes de integração..."
    
    if [ -f "$BUILD_DIR/integration_tests" ]; then
        cd "$BUILD_DIR"
        ./integration_tests --gtest_color=yes --gtest_output=xml:integration_test_results.xml
        print_success "Testes de integração concluídos!"
    else
        print_error "Executável de testes de integração não encontrado!"
        return 1
    fi
}

# Função para gerar relatório de cobertura
generate_coverage() {
    print_info "Gerando relatório de cobertura..."
    
    cd "$BUILD_DIR"
    
    # Verifica se gcov está disponível
    if command -v gcov &> /dev/null; then
        # Coleta dados de cobertura
        find . -name "*.gcda" -exec gcov {} \;
        
        # Se lcov estiver disponível, gera relatório HTML
        if command -v lcov &> /dev/null; then
            lcov --capture --directory . --output-file coverage.info
            lcov --remove coverage.info '/usr/*' --output-file coverage.info
            lcov --remove coverage.info '*/googletest/*' --output-file coverage.info
            
            if command -v genhtml &> /dev/null; then
                genhtml coverage.info --output-directory coverage_html
                print_success "Relatório de cobertura gerado em: $BUILD_DIR/coverage_html/index.html"
            fi
        fi
    else
        print_info "gcov não disponível, pulando relatório de cobertura"
    fi
}

# Função para limpar build
clean_build() {
    print_info "Limpando diretório de build..."
    rm -rf "$BUILD_DIR"
    print_success "Limpeza concluída!"
}

# Função para mostrar ajuda
show_help() {
    echo "Script de automação de testes do projeto IPC"
    echo ""
    echo "Uso: $0 [OPÇÃO]"
    echo ""
    echo "Opções:"
    echo "  unit          Executa apenas testes unitários"
    echo "  integration   Executa apenas testes de integração"  
    echo "  all           Executa todos os testes (padrão)"
    echo "  clean         Limpa diretório de build"
    echo "  coverage      Gera relatório de cobertura"
    echo "  help          Mostra esta ajuda"
    echo ""
    echo "Exemplos:"
    echo "  $0             # Executa todos os testes"
    echo "  $0 unit        # Executa apenas testes unitários"
    echo "  $0 clean       # Limpa build anterior"
}

# Função principal
main() {
    local test_type="${1:-all}"
    
    case "$test_type" in
        "clean")
            clean_build
            exit 0
            ;;
        "help"|"--help"|"-h")
            show_help
            exit 0
            ;;
        "coverage")
            generate_coverage
            exit 0
            ;;
    esac
    
    # Banner
    echo "========================================="
    echo "  Sistema IPC - Execução de Testes"
    echo "========================================="
    echo ""
    
    # Build sempre necessário
    build_tests
    
    # Executa testes baseado no parâmetro
    case "$test_type" in
        "unit")
            run_unit_tests
            ;;
        "integration")
            run_integration_tests
            ;;
        "all"|*)
            run_unit_tests
            echo ""
            run_integration_tests
            ;;
    esac
    
    echo ""
    print_success "Execução de testes finalizada!"
    print_info "Resultados XML salvos em: $BUILD_DIR/"
    
    # Gera cobertura se disponível
    generate_coverage
}

# Executa função principal com todos os argumentos
main "$@"