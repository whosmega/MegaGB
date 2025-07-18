#include <SDL2/SDL.h>
#include <SDL2/SDL_render.h>
#include <SDL2/SDL_video.h>
#include <imgui.h>
#include <backends/imgui_impl_sdl2.h>
#include <backends/imgui_impl_sdlrenderer2.h>
#include <gb/gui.h>
#include <gb/cpu.h>
#include <gb/debug.h>

#include <iostream>
#include <vector>
/* Struct for storing GUI state */

class GuiState {
public:
	bool showInternals;

	/* Settings */
	bool pause;
	float shade0_rgb[3] = {1, 1, 1};
	float shade1_rgb[3] = {0.666, 0.666, 0.666};
	float shade2_rgb[3] = {0.333, 0.333, 0.333};
	float shade3_rgb[3] = {0, 0, 0};
	GuiState();
};

GuiState::GuiState() {
	this->showInternals = false;
	this->pause = false;
}

/* Define Colors */

namespace Color {
	unsigned WinTitleColor = IM_COL32(66, 61, 107, 255);
	unsigned TraceHighlightColor = IM_COL32(153, 78, 89, 255);
}

static std::string disassemble(GB* gb, uint16_t address, int* insLength) {
	char disasm[30];
	uint8_t opcode = readAddr(gb, address);
	if (opcode == 0xCB) {
		disassembleCBInstruction(gb, address+1, (char*)&disasm);
		*insLength = 2;
	} else {
		*insLength = disassembleInstruction(gb, address, (char*)&disasm);
	}

	return std::string(disasm);
}

static void renderInstructionTraceTable(GB* gb) {
	if (ImGui::BeginTable(
				"Instructions", 2, 
				ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_BordersV)) {

		ImGui::TableSetupColumn("Address", 0, 1);
		ImGui::TableSetupColumn("Disassembly", 0, 2);
		ImGui::TableHeadersRow();
		ImGui::TableSetColumnIndex(0);
		ImGui::Text("Address");
		ImGui::TableSetColumnIndex(1);
		ImGui::Text("Disassambly");

		std::vector<std::string> disassembly;
		std::vector<uint16_t> disassemblyAddr;

		/* Insert preceding instructions */
		int currentIndex = gb->dispatchedAddressesStart;
		for (int i=0; i<10; i++) {
			int currentInsLength;
			std::string currentDisasm = disassemble(gb, gb->dispatchedAddresses[currentIndex], &currentInsLength);
			disassembly.push_back(std::string(currentDisasm));
			disassemblyAddr.push_back(gb->dispatchedAddresses[currentIndex]);

			currentIndex++;
			if (currentIndex > 10) currentIndex = 0;
		}

		/* Insert current instruction */
		int currentInsLength;
		uint16_t currentDispatchAddress = gb->dispatchedAddresses[gb->dispatchedAddressesStart > 0 ? gb->dispatchedAddressesStart-1 : 10];
		std::string currentDisasm = disassemble(gb, currentDispatchAddress, &currentInsLength);

		disassembly.push_back(std::string(currentDisasm));
		disassemblyAddr.push_back(currentDispatchAddress);

		/* We are left with 11 instructions, and 2 vectors that give the corresponding disassembly 
		 * and address, we can build forward 9 instructions from this point onwards */
		int relativeIndex = currentInsLength;

		for (int i = 0; i < 9; i++) {
			uint16_t addr = currentDispatchAddress+relativeIndex;
			int insLength = 1;
			std::string disasm = disassemble(gb, addr, (int*)&insLength);

			disassembly.push_back(disasm);
			disassemblyAddr.push_back(addr);

			relativeIndex += insLength;
		}

		/* Disassembly data gathering complete, we can output it now */

		for (int i = 0; i < disassembly.size(); i++) {
			ImGui::TableNextRow();
		
			ImGui::TableSetColumnIndex(0);
			ImGui::Text("0x%04x", disassemblyAddr[i]);

			ImGui::TableSetColumnIndex(1);
			ImGui::Text("%s", disassembly[i].c_str());

			if (i==10) {
				ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, Color::TraceHighlightColor);
			}
		}

		ImGui::EndTable();
	}
}

extern "C" {

int initIMGUI(GB* gb) {
	/* Create EMU State external window, using its own sdl window and renderer, hide it by default */
	int w, h;
	SDL_GetWindowSize(gb->sdl_window, &w, &h);
	SDL_Window* win = SDL_CreateWindow(
		"Emulator State", 
		2*w, SDL_WINDOWPOS_CENTERED, 
		w, h, 
		SDL_WINDOW_HIDDEN
	);
	if (win == NULL) return -1;

	SDL_Renderer* ren = SDL_CreateRenderer(
		win, -1, 
		SDL_RENDERER_PRESENTVSYNC|SDL_RENDERER_ACCELERATED
	);
	if (ren == NULL) return -2;
	gb->imgui_secondary_sdl_window = win;
	gb->imgui_secondary_sdl_renderer = ren;
	
	/* Gui State */
	GuiState* state = new GuiState();
	gb->imgui_gui_state = (void*)state;

	IMGUI_CHECKVERSION();

	/* Setup Main Window Context */
	ImGuiContext* imgui_main_context = ImGui::CreateContext();
	gb->imgui_main_context = imgui_main_context;
	ImGui::StyleColorsDark();

	/* Setup Main Window Backends */
	ImGui_ImplSDL2_InitForSDLRenderer(gb->sdl_window, gb->sdl_renderer);
	ImGui_ImplSDLRenderer2_Init(gb->sdl_renderer);

	/* Setup Secondary Window Context */
	ImGuiContext* imgui_secondary_context = ImGui::CreateContext();
	gb->imgui_secondary_context = imgui_secondary_context;
	ImGui::SetCurrentContext(imgui_secondary_context);
	ImGui::StyleColorsDark();

	/* Setup Secondary Window Backends */
	ImGui_ImplSDL2_InitForSDLRenderer(gb->imgui_secondary_sdl_window, gb->imgui_secondary_sdl_renderer);
	ImGui_ImplSDLRenderer2_Init(gb->imgui_secondary_sdl_renderer);
	return 0;
}

void freeIMGUI(GB* gb) {
	/* Cleanup IMGUI */
	ImGui::SetCurrentContext((ImGuiContext*)gb->imgui_main_context);
	ImGui_ImplSDLRenderer2_Shutdown();
	ImGui_ImplSDL2_Shutdown();
	ImGui::DestroyContext();
	gb->imgui_main_context = NULL;

	ImGui::SetCurrentContext((ImGuiContext*)gb->imgui_secondary_context);
	ImGui_ImplSDLRenderer2_Shutdown();
	ImGui_ImplSDL2_Shutdown();
	ImGui::DestroyContext();
	gb->imgui_secondary_context = NULL;

	SDL_DestroyRenderer(gb->imgui_secondary_sdl_renderer);
	SDL_DestroyWindow(gb->imgui_secondary_sdl_window);
	gb->imgui_secondary_sdl_renderer = NULL;
	gb->imgui_secondary_sdl_window = NULL;
}

void renderFrameIMGUI(GB* gb) {
	/* Renders MENU layered on top of the PPU Graphics on the main renderer, as well as
	 * a different window with its own renderer
	 *
	 * PPU has already written to the main renderer buffer, excluding the area of the menu on top,
	 * We first render into the main menu area, then layer it with any other necessary popups 
	 * that need to be shown */

	/* Preserve context across this function call */
	ImGuiContext* initialCtx = ImGui::GetCurrentContext();


	ImGui::SetCurrentContext((ImGuiContext*)gb->imgui_main_context);
	ImGui_ImplSDLRenderer2_NewFrame();
	ImGui_ImplSDL2_NewFrame();
	ImGui::NewFrame();

	ImGui::SetNextWindowSize(ImVec2(WIDTH_PX*DISPLAY_SCALING, MENU_HEIGHT_PX));
	ImGui::SetNextWindowPos(ImVec2(0, 0));

	ImGuiWindowFlags windowFlags1 = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoResize;
	GuiState* state = (GuiState*)gb->imgui_gui_state;

	ImGui::Begin("Window", NULL, windowFlags1);
	ImGui::SetWindowFontScale(1.8);

	if (ImGui::BeginMenuBar()) {
		if (ImGui::BeginMenu("File")) {
			if (ImGui::MenuItem("Load ROM (.gb/.gbc) [TODO]")) {}
			if (ImGui::MenuItem("Load Save (.gb/.gbc) [TODO]")) {}

			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu("Internal")) {
				if (ImGui::Checkbox("Show Internal", &state->showInternals)) {
					if (state->showInternals) {
						SDL_ShowWindow(gb->imgui_secondary_sdl_window);
					} else {
						SDL_HideWindow(gb->imgui_secondary_sdl_window);
					}
				}

			ImGui::EndMenu();
		}
		ImGui::EndMenuBar();
	}
	
	ImGui::End();
	
	ImGui::Render();
	SDL_RenderSetScale(gb->sdl_renderer, 1, 1);
	ImGui_ImplSDLRenderer2_RenderDrawData(ImGui::GetDrawData(), gb->sdl_renderer);

	if (SDL_GetWindowFlags(gb->imgui_secondary_sdl_window) & (SDL_WINDOW_HIDDEN | SDL_WINDOW_MINIMIZED)) {
		/* If either the imgui window is hidden or minimized, dont render anything 
		 * Restore initial context */
		ImGui::SetCurrentContext(initialCtx);
		return;
	}

	int w, h;
	SDL_GetWindowSize(gb->imgui_secondary_sdl_window, &w, &h);

	ImGui::SetCurrentContext((ImGuiContext*)gb->imgui_secondary_context);
	ImGuiIO io = ImGui::GetIO();
	ImGuiWindowFlags windowFlags2 = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoFocusOnAppearing;

	

	ImGui_ImplSDLRenderer2_NewFrame();
	ImGui_ImplSDL2_NewFrame();
	ImGui::NewFrame();

#define REL_X(r) (r*(w/io.DisplayFramebufferScale.x))    // r=0->1
#define REL_Y(r) (r*(h/io.DisplayFramebufferScale.y))   // r=0->1

	ImGui::PushStyleColor(ImGuiCol_TitleBg, Color::WinTitleColor);
	ImGui::PushStyleColor(ImGuiCol_TitleBgActive, Color::WinTitleColor);
	ImGui::Begin("Registers", NULL, windowFlags2);
	ImGui::PopStyleColor(2);

	ImGui::SetWindowSize(ImVec2(REL_X(0.5), REL_Y(0.5)));
	ImGui::SetWindowPos(ImVec2(REL_X(0), REL_Y(0)));
	ImGui::SetWindowFontScale(1.25);
	
	if (ImGui::BeginTable("Registers", 4, ImGuiTableFlags_BordersV | ImGuiTableFlags_BordersH)) {
		ImGui::TableHeadersRow();
		ImGui::TableSetColumnIndex(0);
		ImGui::Text("Register");
		ImGui::TableSetColumnIndex(1);
		ImGui::Text("Value");
		ImGui::TableSetColumnIndex(2);
		ImGui::Text("Register");
		ImGui::TableSetColumnIndex(3);
		ImGui::Text("Value");

		std::string RegName[8] = {"A","B","C","D","E","F","H","L"};

		for (int row = 0; row < 4; row++) {
			ImGui::TableNextRow();
			
			ImGui::TableSetColumnIndex(0);
			ImGui::Text("%s", RegName[row*2].c_str());

			ImGui::TableSetColumnIndex(1);
			ImGui::Text("0x%02x", gb->GPR[row*2]);

			ImGui::TableSetColumnIndex(2);
			ImGui::Text("%s", RegName[row*2+1].c_str());
			ImGui::TableSetColumnIndex(3);
			ImGui::Text("0x%02x", gb->GPR[row*2+1]);
		}

		ImGui::TableNextRow();

		ImGui::TableSetColumnIndex(0);
		ImGui::Text("PC");

		ImGui::TableSetColumnIndex(1);
		ImGui::Text("0x%04x", gb->PC);

		ImGui::TableSetColumnIndex(2);
		ImGui::Text("SP");
		ImGui::TableSetColumnIndex(3);
		ImGui::Text("0x%04x", (gb->GPR[R16_SP]<<8) | gb->GPR[R16_SP+1]);


		ImGui::EndTable();
	}

	ImGui::End();

	ImGui::PushStyleColor(ImGuiCol_TitleBg, Color::WinTitleColor);
	ImGui::PushStyleColor(ImGuiCol_TitleBgActive, Color::WinTitleColor);
	ImGui::Begin("Settings", NULL, windowFlags2);
	ImGui::PopStyleColor(2);

	ImGui::SetWindowSize(ImVec2(REL_X(0.5), REL_Y(0.5)));
	ImGui::SetWindowPos(ImVec2(REL_X(0), REL_Y(0.5)));
	ImGui::SetWindowFontScale(1.25);	

	/* Color picking and setting for palettes */
	if (ImGui::BeginMenu("DMG Palettes")) {
		ImGui::EndMenu();
	}

	ImGui::ColorEdit3("Shade 0", (float*)&state->shade0_rgb, gb->emuMode == EMU_CGB ? ImGuiColorEditFlags_NoInputs : 0);
	ImGui::ColorEdit3("Shade 1", (float*)&state->shade1_rgb, gb->emuMode == EMU_CGB ? ImGuiColorEditFlags_NoInputs : 0);
	ImGui::ColorEdit3("Shade 2", (float*)&state->shade2_rgb, gb->emuMode == EMU_CGB ? ImGuiColorEditFlags_NoInputs : 0);
	ImGui::ColorEdit3("Shade 3", (float*)&state->shade3_rgb, gb->emuMode == EMU_CGB ? ImGuiColorEditFlags_NoInputs : 0);

	gb->settings.shade0_rgb = (((uint32_t)(state->shade0_rgb[0]*255))<<16)|(((uint32_t)(state->shade0_rgb[1]*255))<<8)|(((uint32_t)(state->shade0_rgb[2]*255)));

	gb->settings.shade1_rgb = (((uint32_t)(state->shade1_rgb[0]*255))<<16)|(((uint32_t)(state->shade1_rgb[1]*255))<<8)|(((uint32_t)(state->shade1_rgb[2]*255)));
	gb->settings.shade2_rgb = (((uint32_t)(state->shade2_rgb[0]*255))<<16)|(((uint32_t)(state->shade2_rgb[1]*255))<<8)|(((uint32_t)(state->shade2_rgb[2]*255)));
	gb->settings.shade3_rgb = (((uint32_t)(state->shade3_rgb[0]*255))<<16)|(((uint32_t)(state->shade3_rgb[1]*255))<<8)|(((uint32_t)(state->shade3_rgb[2]*255)));

	/* ----- Pausing ------ */
	if (ImGui::BeginMenu("Emulator")) {
		ImGui::EndMenu();
	}
	if (ImGui::Checkbox("Pause", &state->pause)) {
		if (state->pause) {
			/* Prematurely terminate current frame rendering as emulator enters pause mode,
			 * after we exit we just return */
			ImGui::End();
			ImGui::SetCurrentContext(initialCtx);
			ImGui::EndFrame();
			pauseGBEmulator(gb);
			return;
		}
		else unpauseGBEmulator(gb);
	}
	/* ------------------------------------- */
	ImGui::End();

	ImGui::PushStyleColor(ImGuiCol_TitleBg, Color::WinTitleColor);
	ImGui::PushStyleColor(ImGuiCol_TitleBgActive, Color::WinTitleColor);
	ImGui::Begin("Instruction Tracer", NULL, windowFlags2);
	ImGui::PopStyleColor(2);

	ImGui::SetWindowSize(ImVec2(REL_X(0.5), REL_Y(1)));
	ImGui::SetWindowPos(ImVec2(REL_X(0.5), REL_Y(0)));
	ImGui::SetWindowFontScale(1.25);

	renderInstructionTraceTable(gb);

	ImGui::End();

#undef REL_SIZE_X
#undef REL_SIZE_Y

	ImGui::Render();

	SDL_RenderSetScale(gb->imgui_secondary_sdl_renderer, io.DisplayFramebufferScale.x, io.DisplayFramebufferScale.y);
	SDL_SetRenderDrawColor(gb->imgui_secondary_sdl_renderer, 48, 48, 48, 255);
	SDL_RenderClear(gb->imgui_secondary_sdl_renderer);
	ImGui_ImplSDLRenderer2_RenderDrawData(ImGui::GetDrawData(), gb->imgui_secondary_sdl_renderer);
	SDL_RenderPresent(gb->imgui_secondary_sdl_renderer);

	/* Restore initial context */
	ImGui::SetCurrentContext(initialCtx);
}

void processEventsIMGUI(GB* gb, SDL_Event* event) {
	if (event->type == SDL_WINDOWEVENT) {
		if (event->window.event == SDL_WINDOWEVENT_CLOSE && 
			event->window.windowID == SDL_GetWindowID(gb->imgui_secondary_sdl_window)) {

			/* Requested secondary window to close, hide it */
			GuiState* state = (GuiState*)gb->imgui_gui_state;
			state->showInternals = false;
			SDL_HideWindow(gb->imgui_secondary_sdl_window);
		} else if (event->window.event == SDL_WINDOWEVENT_FOCUS_GAINED) {
			/* Whichever window gains focus, set current imgui context to it, for
			 * appropriate event handling */
			if (event->window.windowID == SDL_GetWindowID(gb->imgui_secondary_sdl_window)) {
				ImGui::SetCurrentContext((ImGuiContext*)gb->imgui_secondary_context);
			} else {
				ImGui::SetCurrentContext((ImGuiContext*)gb->imgui_main_context);
			}
		}
	}

	ImGui_ImplSDL2_ProcessEvent(event);
}

}
