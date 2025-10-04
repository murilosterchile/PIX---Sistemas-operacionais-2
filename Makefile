# Makefile para Sistema de Transferência de Valores
# INF01151 - Sistemas Operacionais II - Trabalho Prático Etapa 1
# Compilação para ambiente Linux (laboratórios INF-UFRGS)

# Compilador e flags
CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -O2 -pthread
DEBUGFLAGS = -std=c++17 -Wall -Wextra -g -pthread -DDEBUG

# Diretórios
COMMON_DIR = common
SERVER_DIR = server
CLIENT_DIR = client
OBJ_DIR = obj

# Arquivos executáveis
SERVER_TARGET = servidor
CLIENT_TARGET = cliente

# Arquivos fonte comuns
COMMON_SOURCES = $(COMMON_DIR)/utils.cpp
COMMON_HEADERS = $(COMMON_DIR)/protocol.h $(COMMON_DIR)/utils.h $(COMMON_DIR)/server_data.h $(COMMON_DIR)/debug.h

# Arquivos fonte do servidor
SERVER_SOURCES = $(SERVER_DIR)/servidor.cpp \
                 $(SERVER_DIR)/discovery.cpp \
                 $(SERVER_DIR)/processing.cpp \
                 $(SERVER_DIR)/interface.cpp
SERVER_HEADERS = $(SERVER_DIR)/discovery.h \
                 $(SERVER_DIR)/processing.h \
                 $(SERVER_DIR)/interface.h

# Arquivos fonte do cliente
CLIENT_SOURCES = $(CLIENT_DIR)/cliente.cpp \
                 $(CLIENT_DIR)/client_discovery.cpp \
                 $(CLIENT_DIR)/client_processor.cpp \
                 $(CLIENT_DIR)/client_interface.cpp
CLIENT_HEADERS = $(CLIENT_DIR)/client_discovery.h \
                 $(CLIENT_DIR)/client_processor.h \
		 $(CLIENT_DIR)/client_interface.h 

# Objetos
COMMON_OBJECTS = $(COMMON_SOURCES:$(COMMON_DIR)/%.cpp=$(OBJ_DIR)/common_%.o)
SERVER_OBJECTS = $(SERVER_SOURCES:$(SERVER_DIR)/%.cpp=$(OBJ_DIR)/server_%.o)
CLIENT_OBJECTS = $(CLIENT_SOURCES:$(CLIENT_DIR)/%.cpp=$(OBJ_DIR)/client_%.o)

# Target padrão
all: $(SERVER_TARGET) $(CLIENT_TARGET)

# Criar diretório de objetos se não existir
$(OBJ_DIR):
	@mkdir -p $(OBJ_DIR)

# Compilar servidor
$(SERVER_TARGET): $(SERVER_OBJECTS) $(COMMON_OBJECTS) | $(OBJ_DIR)
	@echo "Linkando servidor..."
	$(CXX) $(CXXFLAGS) -o $@ $^
	@echo "Servidor compilado com sucesso!"

# Compilar cliente
$(CLIENT_TARGET): $(CLIENT_OBJECTS) $(COMMON_OBJECTS) | $(OBJ_DIR)
	@echo "Linkando cliente..."
	$(CXX) $(CXXFLAGS) -o $@ $^
	@echo "Cliente compilado com sucesso!"

# Compilar objetos comuns
$(OBJ_DIR)/common_%.o: $(COMMON_DIR)/%.cpp $(COMMON_HEADERS) | $(OBJ_DIR)
	@echo "Compilando $<..."
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Compilar objetos do servidor
$(OBJ_DIR)/server_%.o: $(SERVER_DIR)/%.cpp $(SERVER_HEADERS) $(COMMON_HEADERS) | $(OBJ_DIR)
	@echo "Compilando $<..."
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Compilar objetos do cliente
$(OBJ_DIR)/client_%.o: $(CLIENT_DIR)/%.cpp $(CLIENT_HEADERS) $(COMMON_HEADERS) | $(OBJ_DIR)
	@echo "Compilando $<..."
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Target para compilação com debug
debug: CXXFLAGS = $(DEBUGFLAGS)
debug: clean all
	@echo "Versão de debug compilada!"

# Target para compilar apenas o servidor
servidor: $(SERVER_TARGET)

# Target para compilar apenas o cliente
cliente: $(CLIENT_TARGET)

# Target para executar testes básicos
test: all
	@echo "Executando testes básicos..."
	@echo "Verificando se executáveis foram criados..."
	@test -f $(SERVER_TARGET) && echo "✓ Servidor compilado" || echo "✗ Servidor não encontrado"
	@test -f $(CLIENT_TARGET) && echo "✓ Cliente compilado" || echo "✗ Cliente não encontrado"
	@echo "Para testar o funcionamento, execute:"
	@echo "  Terminal 1: ./$(SERVER_TARGET) 4000"
	@echo "  Terminal 2: ./$(CLIENT_TARGET) 4000"

# Target para executar o servidor em background (útil para testes)
run-server: $(SERVER_TARGET)
	@echo "Iniciando servidor na porta 4000..."
	./$(SERVER_TARGET) 4000

# Target para executar o cliente
run-client: $(CLIENT_TARGET)
	@echo "Iniciando cliente na porta 4000..."
	./$(CLIENT_TARGET) 4000

# Target para verificar sintaxe sem compilar
check:
	@echo "Verificando sintaxe dos arquivos..."
	$(CXX) $(CXXFLAGS) -fsyntax-only $(SERVER_SOURCES) $(CLIENT_SOURCES) $(COMMON_SOURCES)
	@echo "Verificação de sintaxe concluída!"

# Target para mostrar informações de compilação
info:
	@echo "=== Informações de Compilação ==="
	@echo "Compilador: $(CXX)"
	@echo "Flags: $(CXXFLAGS)"
	@echo "Arquivos do servidor: $(SERVER_SOURCES)"
	@echo "Arquivos do cliente: $(CLIENT_SOURCES)"
	@echo "Arquivos comuns: $(COMMON_SOURCES)"
	@echo "================================"

# Target para criar estrutura de diretórios (caso não existam)
setup:
	@echo "Criando estrutura de diretórios..."
	@mkdir -p $(COMMON_DIR) $(SERVER_DIR) $(CLIENT_DIR) $(OBJ_DIR)
	@echo "Estrutura criada!"

# Target para instalar dependências (se necessário no sistema)
install-deps:
	@echo "Verificando dependências do sistema..."
	@which g++ > /dev/null || (echo "ERRO: g++ não encontrado. Instale: sudo apt-get install build-essential" && exit 1)
	@echo "✓ g++ encontrado"
	@echo "✓ Todas as dependências estão disponíveis"

# Limpeza
clean:
	@echo "Limpando arquivos temporários..."
	rm -rf $(OBJ_DIR)
	rm -f $(SERVER_TARGET) $(CLIENT_TARGET)
	rm -f *.o *.core core
	@echo "Limpeza concluída!"

# Limpeza completa (inclui backups e temporários do editor)
distclean: clean
	@echo "Limpeza completa..."
	rm -f *~ $(SERVER_DIR)/*~ $(CLIENT_DIR)/*~ $(COMMON_DIR)/*~
	rm -f *.bak $(SERVER_DIR)/*.bak $(CLIENT_DIR)/*.bak $(COMMON_DIR)/*.bak
	rm -f .*.swp $(SERVER_DIR)/.*.swp $(CLIENT_DIR)/.*.swp $(COMMON_DIR)/.*.swp
	@echo "Limpeza completa concluída!"

# Target para criar arquivo tar.gz para entrega
package: distclean
	@echo "Criando pacote para entrega..."
	@PACKAGE_NAME="trabalho-pratico-etapa1-$$(date +%Y%m%d)" && \
	tar -czf $${PACKAGE_NAME}.tar.gz \
		Makefile README.md \
		$(COMMON_DIR)/ $(SERVER_DIR)/ $(CLIENT_DIR)/ \
		--exclude='*.o' --exclude='*.core' --exclude='*~' \
		--exclude='*.bak' --exclude='.*.swp'
	@echo "Pacote criado: trabalho-pratico-etapa1-$$(date +%Y%m%d).tar.gz"

# Target de ajuda
help:
	@echo "=== Sistema de Transferência de Valores - Makefile ==="
	@echo "Targets disponíveis:"
	@echo "  all          - Compila servidor e cliente (padrão)"
	@echo "  servidor     - Compila apenas o servidor"
	@echo "  cliente      - Compila apenas o cliente"
	@echo "  debug        - Compila versão com debug"
	@echo "  test         - Executa testes básicos"
	@echo "  run-server   - Executa o servidor na porta 4000"
	@echo "  run-client   - Executa o cliente na porta 4000"
	@echo "  check        - Verifica sintaxe sem compilar"
	@echo "  info         - Mostra informações de compilação"
	@echo "  setup        - Cria estrutura de diretórios"
	@echo "  install-deps - Verifica dependências do sistema"
	@echo "  clean        - Remove objetos e executáveis"
	@echo "  distclean    - Limpeza completa (inclui temporários)"
	@echo "  package      - Cria pacote tar.gz para entrega"
	@echo "  help         - Mostra esta ajuda"
	@echo ""
	@echo "Exemplos de uso:"
	@echo "  make                    # Compila tudo"
	@echo "  make debug              # Versão com debug"
	@echo "  make clean all          # Limpa e recompila"
	@echo "  make servidor           # Apenas o servidor"
	@echo "  make test               # Testa compilação"
	@echo "=================================================="

# Declarar targets que não criam arquivos
.PHONY: all clean distclean debug test servidor cliente run-server run-client
.PHONY: check info setup install-deps package help

# Manter arquivos intermediários em caso de erro
.SECONDARY: $(COMMON_OBJECTS) $(SERVER_OBJECTS) $(CLIENT_OBJECTS)

# Regra para recompilação automática quando headers mudam
$(SERVER_OBJECTS): $(COMMON_HEADERS) $(SERVER_HEADERS)
$(CLIENT_OBJECTS): $(COMMON_HEADERS) $(CLIENT_HEADERS)
$(COMMON_OBJECTS): $(COMMON_HEADERS)
