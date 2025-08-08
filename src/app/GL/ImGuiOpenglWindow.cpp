#include "ImGuiOpenglWindow.hpp"
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <OpenGL/imgui_impl_opengl3.h>
#include <GLFW/glfw3.h>

ImGuiOpenglWindow::~ImGuiOpenglWindow(){
}

void ImGuiOpenglWindow::init(GLFWwindow* window){
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 130");
}

void ImGuiOpenglWindow::newFrame() {
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
}

void ImGuiOpenglWindow::render(){
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

void ImGuiOpenglWindow::destroy(){
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
}
