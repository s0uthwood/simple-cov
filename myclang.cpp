#include <iostream>
#include <vector>
#include <string>
#include <cstdlib>
#include <unistd.h>
#include <fstream>
#include <sys/types.h>
#include <sys/wait.h>
#include <sstream>

#include "instrument.h"
#include "config.h"

using namespace llvm;

const std::string COV_RUNTIME = COV_RUNTIME_FILE;

bool is_file_exists(const std::string& filename) {
    std::ifstream file(filename);
    return file.good();
}

std::string get_file_ext(const std::string& filename) {
    size_t pos = filename.find_last_of('.');
    if (pos != std::string::npos) {
        return filename.substr(pos);
    }
    return "";
}

std::string get_file_basename(const std::string& filename) {
    size_t pos = filename.find_last_of('.');
    if (pos != std::string::npos) {
        return filename.substr(0, pos);
    }
    return filename;
}

bool is_src_file(const std::string& filename) {
    std::string ext = get_file_ext(filename);
    return (ext == ".c" || ext == ".cpp" || ext == ".cc" || ext == ".cxx");
}

bool is_obj_file(const std::string& filename) {
    std::string ext = get_file_ext(filename);
    return (ext == ".o" || ext == ".obj");
}

std::string filename_to_ll(const std::string& src) {
    return get_file_basename(src) + ".ll";
}

std::string file_name_to_obj(const std::string& src) {
    return get_file_basename(src) + ".o";
}

int run_command(std::string command, int mode) {
    std::cout << command << std::endl;
    std::string exeTarget;
    if (mode == 0)
        exeTarget = "/usr/bin/clang";
    else 
        exeTarget = "/usr/bin/llc";
    std::istringstream iss(command);
    std::vector<std::string> tokens;
    std::string token;
    while (iss >> token) {
        tokens.push_back(token);
    }
    std::vector<char *> args;
    for (const auto& t : tokens) {
        args.push_back(const_cast<char *>(t.c_str()));
    }
    args.push_back((char *)NULL);
    std::cout << "Executing command: " << exeTarget << std::endl;
    std::cout << "Arguments: ";
    for (const auto& arg : args) {
        std::cout << arg << " ";
    }
    std::cout << std::endl;
    pid_t pid = fork();

    if (pid == -1) {
        std::cerr << "Failed to fork!\n";
        return 1;
    } else if (pid == 0) {
        const int MAX_PATH = 1024;
        char buffer[MAX_PATH];
        if (getcwd(buffer, sizeof(buffer)) != nullptr) {
            std::cout << "Current Working Directory: " << buffer << std::endl;
        }
        if(execv(exeTarget.c_str(), args.data()) == -1) {
            perror("Failed to execv!\n");
            exit(EXIT_FAILURE);
        }
    } else {
        int status;
        waitpid(pid, &status, 0);
        if (WIFEXITED(status)) {
            return WEXITSTATUS(status);
        }
    }
    return 0;
}

int run_clang(const std::vector<std::string>& args) {
    std::string command = "clang";
    for (const auto& arg : args) {
        command += " " + arg;
    }
    return run_command(command, 0);
}

int remove_file(std::string llFile) {
    std::string command = "rm " + llFile ;
    return system(command.c_str());
}

int create_ll_file(std::string cFile, std::string llFile, std::vector<std::string> args) {
    std::string command = "";
    command += "clang -S -emit-llvm " + cFile + " -o " + llFile;
    for (int i = 0; i < args.size(); i++)
        command += " " + args[i];
    system(command.c_str());
    return 0;
}

int create_obj_file(std::string llFile, std::string oFile, std::vector<std::string> args) {
    std::string command = "";
    command += "clang -c " + llFile + " -o " + oFile;
    for (int i = 0; i < args.size(); i++) {
        if (args[i] == "-fsanitize=address")
            continue;
        command += " " + args[i];
    }
    system(command.c_str());
    return 0;
}

int compile_src_to_obj(const std::string& src, const std::vector<std::string>& args) {
    std::string llFile = filename_to_ll(src);
    std::string oFile = file_name_to_obj(src);
    create_ll_file(src, llFile, args);
    instrumentFile(llFile);
    create_obj_file(llFile, oFile, args);
    // remove_file(llFile);  // Uncomment if you want to remove the .ll file after creating the object file
    return 0;
}

int linkobj_files(std::vector<std::string> obj_files, std::string exeFile, std::vector<std::string> args) {
    std::string command = "";
    command += "clang";
    for (int i = 0; i < obj_files.size(); i++)
        command += " " + obj_files[i];
    command += " -o " + exeFile;
    for (int i = 0; i < args.size(); i++)
        command += " " + args[i];
    system(command.c_str());
    for (int i = 0; i < obj_files.size(); i++)
        if (obj_files[i] != COV_RUNTIME)
            remove_file(obj_files[i]);
    return 0;
}

int link_final_executable(const std::vector<std::string>& objs, const std::string& output, const std::vector<std::string>& args) {
    std::vector<std::string> allObjs = objs;
    allObjs.push_back(COV_RUNTIME);
    return linkobj_files(allObjs, output, args);
}

int main(int argc, char** args) {
    std::vector<std::string> src_files;
    std::vector<std::string> obj_files;
    std::vector<std::string> other_args;
    std::string output_file = "";
    std::string command = "";
    int link_flag = 1;
    int eflag = 0;

    for (int i = 1; i < argc; i++) {
        std::string arg = std::string(args[i]);

        if (arg == "-o" && i + 1 < argc) {
            output_file = std::string(args[++i]);
        } else if (arg == "-c") {
            link_flag = 0;
        } else if (arg == "-E") {
            eflag = 1;
        } else if (is_src_file(arg)) {
            if (!is_file_exists(arg)) {
                std::cerr << "Error: Source file '" << arg << "' not found" << std::endl;
                return 1;
            }
            src_files.push_back(arg);
        } else if (is_obj_file(arg)) {
            obj_files.push_back(arg);
        } else {
            other_args.push_back(arg);
        }
    }

    if (eflag) {
        std::vector<std::string> e_cmd_args;
        for (int i = 1; i < argc; i++)
            e_cmd_args.emplace_back(args[i]);
        return run_clang(e_cmd_args);
    }

    if (src_files.empty() && obj_files.empty())
        return run_clang(other_args);  // header-only compilation

    // .c -> .ll -> instrument -> .o
    for (const auto&src : src_files) {
        compile_src_to_obj(src, other_args);
    }

    if (link_flag) {
        if (output_file.empty())
            output_file = "a.out";  // Default output file if not specified
        obj_files.insert(obj_files.end(), file_name_to_obj(src_files[0]));
        return link_final_executable(obj_files, output_file, other_args);
    } else {
        // -c + -o xxx.o
        if (src_files.size() == 1 && output_file != "") {
            std::string llFile = filename_to_ll(src_files[0]);
            create_ll_file(src_files[0], llFile, other_args);
            instrumentFile(llFile);
            create_obj_file(llFile, output_file, other_args);
            return 0;
        }
        std::cerr << "Unsupported combination of arguments for -c" << std::endl;
        return 1;
    }

    return 0;
}
