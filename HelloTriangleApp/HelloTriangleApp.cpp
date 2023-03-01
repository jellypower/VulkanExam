#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>


#include <iostream>
#include <stdexcept>
#include <cstdlib>

#include <vector>
#include <cstring>
#include <optional>
#include <set>
#include <algorithm>
#include <string>
#include <fstream>


#include <glm/glm.hpp>
/*
    여기부터

*/

struct Vertex {
    glm::vec2 pos;
    glm::vec3 color;
};

VkResult CreateDeubgUtilMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo,
    const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pDebugMessenger) {
    auto func =
        (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
    // debug messenger를 만들기 위해선 debug messenger create info를 vkCreateDebugUtilsMessengerEXT 함수에
    // 전달해줘야 하는데 vkCreateDebugUtilsMessengerEXT 함수는 extension이기 때문에 자동으로 load되지 않아서
    // 직접 load해줘야 한다. (vkGetInstanceProcAddr함수: 커맨드로 얻어올 수 있는 모든 함수에 대한 포인터)

    if (func != nullptr) {
        return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
    }
    else {
        return VK_ERROR_EXTENSION_NOT_PRESENT;
    }

}

void DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger,
    const VkAllocationCallbacks* pAllocator) {
    auto func =
        (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");

    if (func != nullptr) {
        func(instance, debugMessenger, pAllocator);
        
    }
}

static std::vector<char> readFile(const std::string& filename) {
    std::ifstream file(filename, std::ios::ate | std::ios::binary);

    if (!file.is_open()) {
        throw std::runtime_error("failed to open file!");
    }

    size_t fileSize = (size_t)file.tellg();
    std::vector<char> buffer(fileSize);

    file.seekg(0);
    file.read(buffer.data(), fileSize);

    file.close();

    return buffer;
}

 
class HelloTriangleApplication {
public:
    void run() {
        initWindow();
        initVulkan();
        mainLoop();
        cleanup();
    }

private:

    GLFWwindow* window;
    VkInstance instance;
    // vulkan에선, debugmessenger마저 handle을 이용해 명시적으로 생성해주고 파괴해줘야 한다.
    // 이를 위해서 class멤버로 debugMessenger를 선언해줘야 한다.
    VkDebugUtilsMessengerEXT debugMessenger;

    // vulkan은 플랫폼에 독립적(agnostic)하기 때문에 glfw가 만든 windw에 바로 접근하지 못하고 window에 접근하기 위한
    // 별도의 surface레이어가 필요로 된다.
    VkSurfaceKHR surface;
    VkSwapchainKHR swapChain;
    std::vector<VkImage> swapChainImages;
    VkFormat swapChainImageFormat;
    VkExtent2D swapChainExtent;
    std::vector<VkImageView> swapChainImageViews; // VkImageView의 각각의 element는 각각의 최종 output attachment와 대응된다.

    // 파이프라인을 통해 uniform변수의 값을 바꿔주는 등의 셰이더, vertex 데이터 접근 가능
    VkRenderPass renderPass;
    VkPipelineLayout pipelineLayout;
    // VkPipelineLayout은 셰이더 스테이지와 셰이더 리소스 사이의 인터페이스를 기술하는데 사용됨 (ex. 텍스쳐, 정점, 유니폼변수 등등)

    std::vector<VkFramebuffer> swapChainFrameBuffers;
    
    VkCommandPool commandPool;

    //VkCommandBuffer commandBuffer;
    //VkSemaphore imageAvailableSemaphore; // swapchain으로부터 이미지를 얻어왔다는 것에 대한 signal을 보내는 세마포어
    //VkSemaphore renderFinishedSemaphore; // 렌더링이 끝났고 present가 가능하다는 것에 대한 signal을 보내는 세마포어
    //VkFence inFlightFence; // 한 번에 하나의 프레임만 렌더링 되도록 하는 세마포어

    std::vector<VkCommandBuffer> commandBuffers;

    std::vector<VkSemaphore> imageAvailableSemaphores; // swapchain으로부터 이미지를 얻어왔다는 것에 대한 signal을 보내는 세마포어
    std::vector<VkSemaphore> renderFinishedSemaphores; // 렌더링이 끝났고 present가 가능하다는 것에 대한 signal을 보내는 세마포어
 
    std::vector<VkFence> inFlightFences; // 한 번에 하나의 프레임만 렌더링 되도록 하는 세마포어

    bool framebufferResized = false;



    const uint32_t WIDTH = 800;
    const uint32_t HEIGHT = 600;

    const std::vector<const char*> validationLayers = { "VK_LAYER_KHRONOS_validation" };
    const std::vector<const char*> deviceExtensions = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };

    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;


    VkDevice device;
    VkQueue graphicsQueue;
    // 큐는 기본적으로 logical device를 생성하면 같이 만들어지지만
    // 생성만 되고 우리가 이에 대한 handle을 아직 가지고 있지 않기 때문에
    // 이를 명시적으로 가져와 줘야 한다.
    VkQueue presentQueue;

    VkPipeline graphicsPipeline;


    const int MAX_FRAMES_IN_FLIGHT = 2;

    uint32_t currentFrame = 0;

#ifdef NDEBUG
    const bool enableValidationlayers = false;
#else
    const bool enableValidationlayers = true;
#endif

    bool checkValidationLayerSupport() {
        uint32_t layerCount;

        vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

        std::vector<VkLayerProperties> availableLayers(layerCount);
        vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

        

        std::cout << "\n\n";

        for (const char* layerName : validationLayers) {
            bool layerFound = false;

            for (const auto& layerProperties : availableLayers) {

                if (strcmp(layerName, layerProperties.layerName) == 0) {
                    layerFound = true;
                    break;
                }


            }

            if (!layerFound) {
                return false;
            }

        }

        return true;

    }

    std::vector<const char*> getRequiredExtensions() {
        uint32_t glfwExtensionCount = 0;
        const char** glfwExtensions;
        glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

        std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);

        if (enableValidationlayers) {
            extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
            // validation layer 자체를 enable하는데 필요한 extension을 로딩
        }

        return extensions;

    }

    // VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity: 메세지의 severity의 정도를 나타내는 파라미터
    // VkDebugUtilsMessageTypeFlagsEXT messageType: 메세지의 타입 \
           // 1) 성능, 스펙과 관계 없는 event 발생 \
           // 2) 성능, 스펙을 어기는 event 발생 \
           // 3) vulkan의 non optimal한 usage
    // const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData: struct형태로 메세지의 디테일을 나타내줌
    // void* pUserData: pUserData를 이용하면 callback의 setup단계에서 정의되며 이를 통해 데이터를 보내줄 수 있다.
    static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
        VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, // 메세지의 severity의 정도를 나타내는 파라미터
        VkDebugUtilsMessageTypeFlagsEXT messageType, // 메세지의 타입: 
        const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
        void* pUserData) {
        std::cerr << "validation layer: " << pCallbackData->pMessage << std::endl;


        return VK_FALSE;
        // callback의 return값은 boolean으로 만약 vulkan이 call한 layer메세지가 중단돼야하는지를 결정한다.
        // 만약 callback이 true를 return하면 호출은 중단되고 VK_ERROR_VALIDATION_FAILED_EXT에러를 던진다.
        // 보통 validation layer자체가 오류가 있는지를 판단하기 위해 사용되기때문에 보통은 항상 false로 설정해야 한다.
    }

private:

    void initWindow() {

        glfwInit();

        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        
        window = glfwCreateWindow(WIDTH, HEIGHT, "Vulkan", nullptr, nullptr);
        glfwSetWindowUserPointer(window, this);
        glfwSetFramebufferSizeCallback(window, frameBufferResizeCallback);

    }

    // Chapter: Drawing a triangle -> Drawing -> Frames in flight -> Handling resizes explicitly
    // 많은 드라이버들과 플랫폼들이 윈도우를 resize한 이후VK_ERROR_OUT_OF_DATE_KHR을 자동적으로 trigger한다고 하더라도 
    // 동작이 보장되는 것은 아닙니다. 그것이 우리가 추가적인 핸들 리사이즈 관련 콜백을 명시적으로 지정해줘야 하는 이유입니다.
    // 일단 클래스의 private멤버변수로 resize가 일어났음을 알려주는 플래그 멤버 변수를 추가해줍니다.
    // bool framebufferResized = false;
    // 이후 drawFrame에서 두 번째 recreateSwapChain() 함수 호출 부분으로 가서 해당 flag를 체크하도록 로직을 수정해줍니다.
    // 
    // 굳이 첫 번째가 아닌 두 번째 vkQueuePresentKHR함수를 실행한 이후 flag를 체크하는 세마포어가 일관된
    // 상태를 유지하기 위해 꽤나 중요합니다. 그게 아니라면 시그널된(사용 가능한) 세마포어의 상태를 제대로 얻어올 수 없을
    // 수도 있거든요.
    // 이제 실제로 resize를 감지해주기 위해 GLFW프레임워크의 glfwSetFramebufferSizeCallback 함수를 이용하여
    // 콜백 함수를 등록해 줍시다.   
    //

    static void frameBufferResizeCallback(GLFWwindow* window, int width, int hegith) {
        // 지금 static 함수로 콜백을 만든 이유는 GLFW는 어떻게 this포인터의 멤버함수를 호출할지에 대해 모르기 때문입니다.
        // GLFW에는 현재 만들고 있는 HelloTriangleApp 클래스가 없잖아요? 게다가 glfw는 C언어 기반이라서
        // C++ 기반의 멤버함수를 콜백함수로 넘겨주지도 못하구요
        // 
        // 그러나 콜백함수를 통해GLFWwindow를 callback함수의 파라미터로 얻어올 수 있다는 것을 보셨을 테고
        // 이와 함께 유저가 가진 임의의 포인터를 콜백에게 전달해주는 glfwSetWindowUserPointer라는 
        // GLFW내장 함수가 있으니 그걸 통해 유저 데이터를 해당 콜백에게 넘겨 줄 수 있습니다.
        // initWindow 함수로 가서 glfwSetWindowUserPointer(window, this); 구문을 추가해주도록 합시다.
        // 
        // 이제 넘겨준 해당 값을 이용하기 위해서 콜백함수 내에서 glfwGetWindowUserPointer함수를 써서 해당 주소값을
        // 얻어오고 이를 캐스팅하여 활용하면 됩니다.
        //
        
        auto app = reinterpret_cast<HelloTriangleApplication*>(glfwGetWindowUserPointer(window));
        app->framebufferResized = true;

        // 이제 프로그램을 실행해보고 프레임 버퍼가 실제로 리사이즈동작을 제대로 하는지 확인해 봅시다.
        // 맞다, resize를 하려면 glfwInit함수로 가서 GLFW_RESIZABLE힌트에 대한 기능을 꺼야하는 것을 잊지 마세요.


        // swap chain이 out of date가 되는 특별한 경우가 또 있습니다. 바로 프레임버퍼 사이즈가 0이 되는 상황입니다.
        // 맞아요, 윈도우 창을 최소화 하면 일어나는 일이죠. 아니면 윈도우 창을 최소 사이즈로 resize해버린다던가요.
        // 이번 튜토리얼에선 그냥 위도우가 다시 foreground로 나올 때까지 프로그램을 일시정지 상태로 둘겁니다.
        // recreateSwapChain 함수의 처음 부분으로 이동해보도록 하죠.


    }

#pragma region initVulkan
    void initVulkan() {
        createInstance();
        setupDebugMessanger();
        createSurface();
        pickPhysicalDevice();
        createLogicalDevice();
        createSwapChain();
        createImageViews();
        createRenderPass();
        createGraphicsPipeline();
        createFrameBuffers();
        createCommandPool();
        createCommandBuffers();
        createSyncObjects();

        // VkDeviceMemory: 그냥 V-RAM에 메모리를 할당하는 것
        // VkImage: 해당 메모리를 어떻게 swapchain의 이미지로 사용하는지에 대한
        //  포맷의 정의를 기술하는 것을 포함한 개념(스왑체인의 이미지를 대변함). (ex. 이미지는 2D이고, 포맷은R8G8B8, extent는 2*2 등등)
        // VkImageView: VkImage의 어느 부분을 사용할지 기술함. incompatiable한 interface의 포맷을 맞춰주는 역할도 할 수 있음
        // Framebuffer: VkImageView와 렌더패스로부터 출력된 attachment를 bind해주는 역할을 수행함.
        // VkRenderPass: 어느 attachment가 그려질지를 결정함.
        // https://stackoverflow.com/questions/39557141/what-is-the-difference-between-framebuffer-and-image-in-vulkan


    }

    void createSyncObjects() {
        // 현재 우리는 3가지 기능이 필요합니다.
        // swapchain으로부터 이미지를 얻어왔다는 것에 대한 signal을 보내는 세마포어
        // 렌더링이 끝났고 present가 가능하다는 것에 대한 signal을 보내는 세마포어
        // 한 번에 하나의 프레임만 렌더링 되도록 하는 세마포어
        // 클래스 멤버로 선언해 줍시다.

        imageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
        renderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
        inFlightFences.resize(MAX_FRAMES_IN_FLIGHT);

        VkSemaphoreCreateInfo semaphoreInfo{};
        semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

        VkFenceCreateInfo fenceInfo{};
        fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {

            if (vkCreateSemaphore(device, &semaphoreInfo, nullptr, &imageAvailableSemaphores[i]) != VK_SUCCESS ||
                vkCreateSemaphore(device, &semaphoreInfo, nullptr, &renderFinishedSemaphores[i]) != VK_SUCCESS ||
                vkCreateFence(device, &fenceInfo, nullptr, &inFlightFences[i]) != VK_SUCCESS) {
                throw std::runtime_error("failed to create semaphores!");
            }
        }
        // 위 코드들은 항상 나오던 방식대로 작동하니 설명하지 않겠습니다.



    }

    void createCommandBuffers() {

        commandBuffers.resize(MAX_FRAMES_IN_FLIGHT);

        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.commandPool = commandPool;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        // VK_COMMAND_BUFFER_LEVEL_PRIMARY: 해당 commandBuffer는 execution을 위해 다른 큐에 전송될 수 있음. 그러나 다른 command buffer들로부터
        // 호출되는 것은 불가능
        // VK_COMMAND_BUFFER_LEVEL_SECONDARY: 곧바로 execution을 위해 큐에 전송될 순 없음. 그러나, primary command buffer들로부터는 호출 될 수 있음
        allocInfo.commandBufferCount = (uint32_t)commandBuffers.size();
        // 지금같은 경우에는 secondary command buffer를 만들진 않을거지만, common operation을 재사용 하는데 secondary command buffer는 큰 도움이 될 수 있음
        // 지금같은 경우에는 커맨드 버퍼를 하나만 만들거임

        if (vkAllocateCommandBuffers(device, &allocInfo, commandBuffers.data()) != VK_SUCCESS) {
            throw std::runtime_error("failed to allocate command buffers!");
        }


    }
    
    
    
    void createCommandPool() {
        // Vulkan에서 drawing operation과 memory transfer같은 command는 function call을 통해 direct하게 바로바로 실행되는 게 아니라
        // 모든 operation을 기록하고 한 번에 보내진다. 커맨드가 모여서 한 번에 보내지기 때문에 더 효율적으로 작동 할 수 있다.
        // 게다가, 해당 커맨드는 멀티스레드에서 명령을 기록하게 만들 수도 있다.

        // command pool: command buffer를 만들기 전에 command pool을 만들어야 한다. 
        // 커맨들 풀은 버퍼를 저장하고 할당되는 메모리 자체를 manage한다.

        QueueFamilyIndices queueFamilyIndices = findQueueFamilies(physicalDevice);
        VkCommandPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        // VK_COMMAND_POOL_CREATE_TRANSIENT_BIT: 커맨드 풀으로부터 할당된 command buffer가 짧은 life time을 가지게 한다.
        // 커맨드 버퍼가 짧은 시간 내에 reset, free 될 수 있음을 의미한다. 보통 풀 내에서의 메모리 할당을 구현하기 위해 사용된다.
        // VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT: 해당 풀으로부터 할단된 어떤 커맨드 버퍼든 개별적으로 initial state로 돌아갈
        // 수 있고 기록될 수 있게 한다.(가령 vkResetCommandBuffer를 호출하거나, vkBeginCommandBuffer가 호출되면 암시적으로) 해당 플래그가 없으면
        // 커맨드 버퍼들은 다 같이 reset돼야 한다.

        // 우리는 command buffe가 매 프레임마다 기록되길 원하기 때문에 두 번째 flag만 활성화 한다.
        
        // 커맨드 버퍼들은 지난번에 만든 (graphics, presentation)device queue들중에 하나에게 커맨드 버퍼를 전송함으로써 실행됨
        // 각각의 command pool은 ""하나의 타입의 큐""(graphics용이든 presentation용이든)에 대해서만 command buffer들을 할당 할 수 있음
        // 커맨드 풀은 큐마다 하나만 생성 가능하고 커맨드 버퍼는 커맨드 풀 하나에 여러개가 종속돼있음 커맨드 풀은 실제 메모리와 커맨드 버퍼를 매니징
        // 우리는 drawing을 위한 command를 record할 것이기 때문에 graphics queue family를 골랐음
        poolInfo.queueFamilyIndex = queueFamilyIndices.graphicsFamily.value();

        if (vkCreateCommandPool(device, &poolInfo, nullptr, &commandPool) != VK_SUCCESS) {
            throw std::runtime_error("failed to create command pool!");
        }


    }

    void createFrameBuffers() {
        // 하나의 framebuffer 오브젝트는 모든 VkImageView에 대해 레퍼런스 한다.
        // 각각의 VkImageView는 각각의 attachment에 대응된다.
        // 지금과 같은 경우에 attachment에 사용해야 하는 이미지는 어느 이미지를 swapchain에 반환해야 하는지에 달려있다.
        // 이것은 우리가 모든 이미지에 대해 framebuffer를 생성하고 drawing time에 추출된 이미지에 해당하는 프레임 버퍼를 가져오면
        //된다.
        swapChainFrameBuffers.resize(swapChainImageViews.size());

        for (size_t i = 0; i < swapChainImageViews.size(); i++) {
            VkImageView attachments[] = {
                swapChainImageViews[i] // 일련의 image view를 하나의 frame buffer에 넣는다.
            };

            VkFramebufferCreateInfo framebufferInfo{};
            framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
            framebufferInfo.renderPass = renderPass; // frame buffer가 어느 렌더패스에 대응되는지 정의
            framebufferInfo.attachmentCount = 1; 
            framebufferInfo.pAttachments = attachments; // attachment description에 바인딩돼야 하는 imageView 객체들 지정
            framebufferInfo.width = swapChainExtent.width;
            framebufferInfo.height = swapChainExtent.height; 
            framebufferInfo.layers = 1; // image array의 개수

            if (vkCreateFramebuffer(device, &framebufferInfo, nullptr, &swapChainFrameBuffers[i]) != VK_SUCCESS) {
                throw std::runtime_error("failed to create framebuffer!");
            }
        }
        
    }

    void createRenderPass() {

//================================================= attachment =======================================================
//Render-pass: 우리가 직접 정의한 drawing command의 일반적인 정의을 의미함.
//그리고 drawing command는 렌더링 과정에서 어떤 리소스를 사용하는가에 따라 나뉨
//
// 
//Sub-pass: 각각의 render pass는 하나 이상의 과정을 거치는데 이러한 각 과정들을 sub-pass라고 부름
//         가령, 디퍼드 렌더링을 위해선 G-buffer생성을 위한 Depth 렌더링, 그림자 처리를 위한 Shadow-Depth,
//         G-buffer생성을 통한 Albedo, Normal값 출력, 디퍼드 렌더링의 Light처리 각각이 sub pass가 될 수 있다.
//         https://lifeisforu.tistory.com/462
//
// 
//Attachment: 
// 렌더패스의 리소스는 렌더 타겟(color, depth/stancil, resolve)과 input data(같은 렌더 패스의 이전 sub-pass의 결과 등을 포함)
// 가 있음 이러한 렌더패스의 리소스들을 의미하는 것이 바로 attachment임(discriptor, texture, sampler들을 포함하지 않는 개념)
//          attachment는 여러개의 type이 있음
//              1) Color / Depth Attachment: 여기에 무언가를 그리고 나중에 이놈들로부터 읽어올 수 있음. output임
//              2) Input Attachment: 이놈들은 output이 아닌 input으로 사용됨. 가령 디퍼드 렌더링의 G-Buffer를 예로 들 수 있음
//                  G-Buffer에는 albedo, normal, depth등의 이미지를 그릴텐데 일단 subpass가 끝나면 다른 서브패스를 실행
//                  가능한데 이러한 input attachment를 input으로 줄 수 있다.
//                  
//                  Sampler와 attachment의 차이는 원하는 주소를 직접 addressable하지 못한다는 것이다. 현재 작업하고 있는
//                  픽셀에 대한 데이터만 접근 가능하다. 그리고 이건 드라이버가 자체적으로 최적화 하기에 좋은 기회를 준다.
//https://stackoverflow.com/questions/46384007/what-is-the-meaning-of-attachment-when-speaking-about-the-vulkan-api          
//https://lifeisforu.tistory.com/462
// https://lifeisforu.tistory.com/category/Vulkan%20%26%20OpenGL 
// 
        // pipeline을 만들기 전에, 우리는 vulkan에게 렌더링과정에서 사용될 framebuffer attachment가 무엇이 있는지 
        // 알려야 한다. 얼마나 많은 color/depth buffer가 있는지, 얼마나 많은 샘플들이 사용될지
        // 어떻게 rendering operation을 통해 이들의 contents가 관리돼야 하는지를 기술해야 한다.
        // 그리고 이러한 모든 과정들은 render pass object에 warpping 돼있다.

        // single color buffer attachment를 만들 것이다.


        VkAttachmentDescription colorAttachment{}; // => fragment shader의 "layout(location = 0) out vec4 outColor" 에 대응됨
        colorAttachment.format = swapChainImageFormat;
        // color attachment의 format은 swap chain image의 format과 동일해야 함
        colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
        // multisampling을 지금은 안쓰고 있기 때문에 샘플링은 1번
        colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        // loadOp와 storeOp는 렌더링 전후에 attachment에서 뭘 할지를 결정한다.(렌더패스의 시작과 끝에서 어떤 operation을 할지 결정)
        // VK_ATTACHMENT_LOAD_OP_LOAD: attachment의 exixting contents를 보존한다.
        // VK_ATTACHMENT_LOAD_OP_CLEAR: 시작시에 attachment의 값을 clear한다
        // VK_ATTACHMENT_LOAD_OP_DONT_CARE : existing contents가 정의돼있지 않다. existing contents를 신경쓰지 않는다.
        // 
        // VK_ATTACHMENT_STORE_OP_STORE: 렌더된 콘텐츠는 메모리에 저장되고 나중에 읽어질 수 있음
        // VK_ATTACHMENT_STORE_OP_DONT_CARE: framebuffer의 콘텐츠는 렌더링 operation이후에 정의되지 않았음을 명시

        colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        // 위의 loadOp, storeOp는 color와 depth buffer에 대한 내용이고 stencil에 대한 operation은 별도로 저장해줘야 한다.


        // vulkan에서 framebuffer와 texture는 특정 픽셀 포맷의 VkImage 오브젝트로 표현된다.
        // 그러나 메모리의 픽셀 레이아웃은 이미지로 무엇을 하는가에따라 바뀔 수 있다.
        // VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL: 이미지가 color attachment로 쓰인다
        // VK_IMAGE_LAYOUT_PRESENT_SRC_KHR: 스왑체인에서 바로 사용되는 이미지
        // VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL : 메모리 copy operation의 목적지가 될 이미지
        // 이미지는 다음 작업 혹은 각 작업에 적합한 레이아웃으로 전환돼야 한다.
        // 이 내용은 texturing파트에서 더 자세히 다룰 것
        colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        // render pass가 시작하기 전에 이미지가 어떤 레이아웃을 가질지 결정
        // VK_IMAGE_LAYOUT_UNDEFINED는 이전 이미지가 어떤 레이아웃이든 신경쓰지 않는다는 것이다.
        // 근데 그렇다고 clear를 한다는 의미는 아니다.
        colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        // render pass가 끝나면 자동적으로 전환할 레이아웃을 결정
        // VK_IMAGE_LAYOUT_PRESENT_SRC_KHR는 present에 바로 사용될 수 있는 layout이며
        // 이를 finalLayout으로 설정하면 렌더링 이후에 스왑체인을 이용해 렌더링된 이미지가 바로 출력될 수 있게 해준다.


//==============================================render pass=============================================================

        // 하나의 render pass는 여러 sub pass로 이뤄질 수 있다. subpass들은 연속적인 렌더링 operation이며
        // 이전 pass의 framebuffer콘텐츠에 종속적이다.
        // 가령, 여러 post-processing을 하나의 프로세싱이 끝난 이후에 적용하는 것과 같다.
        // 만약 렌더링 operation의 그룹을 하나의 render pass로 묶는다면 vulkan은 알아서 operation을 reorder하고 
        // 메모리 대역폭을 아낄 수 있다. 하지만 우리는 일단 1개의 render pass를 쓸 것이다.

        // 모든 sub-pass는 이전 섹션에서 구조체로 기술했던 attachment중 하나 이상의 attachment를 참조함
        // 어떤 attachment를 참조할지 기술하는 것이 바로 VkAttachmentReference임 
        VkAttachmentReference colorAttachmentRef{};
        colorAttachmentRef.attachment = 0; // attachment description array에서 어느 index를 참조할지 
        colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        // 해당 VkAttachmentReference를 사용하는 서브패스동안에 attachment에 포함할 레이아웃을 지정
        // 해당 서브패스가 시작하면 attachment의 layout을 이것으로 바꿈

        

        //sub-pass 정의
        VkSubpassDescription subpass{};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        // comput shader를 포함해 디자인 됐기 때문에 graphics에 사용할 거라고 명시적으로 정의해줘야함
        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments = &colorAttachmentRef;
        // 이 array에서 attachment의 index는 fragment shader에서 reference됩니다.
        // 추가적으로 subpass에서 설정 가능한 파라미터들
            //pInputAttachments: 셰이더로부터 읽어오는 attachment들
            //pResolveAttachments: 멀티샘플링에 사용되는 attachment들
            //pDepthStencilAttachment : stencil데이터용 attachment들
            //pPreserveAttachments : subpass에서 사용되진 않지만 보존돼야 하는 데이터들


        // drawFrame() 함수로부터 넘어와 여기에 작성을 시작합니다.
        VkSubpassDependency dependency{};
        dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
        dependency.dstSubpass = 0;
        // 처음 두 파라미터는 dependency와 dependent subpass의 index입니다.
        // VK_SUBPASS_EXTERNAL는 특별한 값으로 srcSubpass, dstSubpass인지에 따라서
        // 렌더패스 직전/직후의 "암시적인" 서브패스를 의미합니다.
        // 아까전에 subpass가 하나밖에 없어보여도 subpass 직전/직후의 이러한 transition들 또한
        // "암시적으로" subpass로 간주된다고 했었죠? 렌더 패스 자체로 들어오고 나가는 transition들이 바로 이겁니다.
        // dstSubpass = 0 이 의미하는 것은 우리가 방금 딱 하나 만들었던 하나뿐인 subpass를 의미합니다.
        // subpass를 배열로 만들 수 있다는거 기억하죠? 우리는 subpass를 하나만 만들었고 거기서 0번째 subpass를 레퍼런스 하겠다는 뜻이죠
        // dstSubpass는 항상 srcSubpass보다 커야합니다. (subpass중 하나가 VK_SUBPASS_EXTERNAL로 설정된게 아니라면요)
        dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependency.srcAccessMask = 0;
        // srcStageMask는 모든 vulkan stage들에 대한 bitmask입니다.
        // srcStageMask 필드는 어떤 stage들의 동작이 완료되기를 기다려야 하는지를 기술합니다.
        // dstSubpass로 넘어가기 전에, 해당 bitmask의 동작들을 srcSubmask 내에서 완료하길 바라며 대기하도록 합니다.
        // 우리는 swapChain이 우리가 이미지에 접근하기 전에 이미지를 읽어오는 완료하길 원하기에 위와 같이 설정합니다.
        // 즉, srcSubpass가 color attachment 단계를 완료할 때까지 대기한다는 뜻입니다.
        
        dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        // dstStageMask는 모든 vulkan stage들에 대한 bitmask입니다.
        // 해당 bitmask에 설정된 stage는 srcStageMask에서 설정한 stage들이 모두 끝나기 전에는 실행되지 않습니다.
        // 
        // 다시말해, srcSubpass에서 srcStageMask에 해당된 동작들이 완료되기 전에는
        // dstStageMask에 지정한 동작들이 dstSubpass에서 실행되지 않습니다.
        // 쉽게말해, srcStageMask의 동작이 끝나야 dstStageMask의 동작은 실행 될 수 있지만
        // dstStageMask에 지정되지 않았으면 srcSubpass든 dstSubpass든 병렬적으로 실행된다는 소리죠
        // 
        // 위의 예제에선, 실제로 필요한 순간까지 transition이 일어나지 않게 막습니다.

        // https://www.reddit.com/r/vulkan/comments/s80reu/subpass_dependencies_what_are_those_and_why_do_i/
        // 보충설명 자료

        VkRenderPassCreateInfo renderPassInfo{};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        renderPassInfo.attachmentCount = 1; 
        renderPassInfo.pAttachments = &colorAttachment; 
        renderPassInfo.subpassCount = 1;
        renderPassInfo.pSubpasses = &subpass;

        // dependency를 설정해 줍시다.
        renderPassInfo.dependencyCount = 1;
        renderPassInfo.pDependencies = &dependency;
        

        if (vkCreateRenderPass(device, &renderPassInfo, nullptr, &renderPass) != VK_SUCCESS) {
            throw std::runtime_error("failed to create render pass!");
        }


    }

    void createGraphicsPipeline() {
        auto vertShaderCode = readFile("vert.spv");
        auto fragShaderCode = readFile("frag.spv"); //SPIR-V byte code를 읽어오고

        VkShaderModule vertShaderModule = createShaderModule(vertShaderCode);
        VkShaderModule fragShaderModule = createShaderModule(fragShaderCode);
        // 파이프라인에 셰이더 코드를 넘겨주기 위해서는 이것을 Shader module으로 감싼 object를 넘겨줘야만 한다.


        // 실제로 셰이더를 활용하기 위해선 셰이더코드들을 pipeline stage에 할당해야 한다.
        VkPipelineShaderStageCreateInfo vertShaderStageInfo{};
        vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
        vertShaderStageInfo.module = vertShaderModule; // 넘겨줄 shader module
        vertShaderStageInfo.pName = "main";
        // invoke할 함수 이름(entry point)
        // entry point를 정해줄 수 있다는 것은 즉, 여러 fragment를 single shader module에 묶어서 
        // 다른 entry point를 설정하여 행동을 바꿔 줄 수 있다는 것이다.


        // pSpecializationInfo: 현재 쓰이고 있지는 않지만 나름 중요한 파라미터
        // shader의 constant의 value를 지정할 수 있게 해준다.
        // 이를 통해 하나의 shaderModule을 사용하면서도 다른 constant value를 지정하는 파이프라인이 생성 가능하다.
        // 이렇게 하면 render time에 variable의 value를 지정하는 것보다 optimizing이 가능한데
        // 가령 if문의 경우에 이렇게 constant value를 지정하면 이에 종속된 if statement를 없앨 수 있다.


        VkPipelineShaderStageCreateInfo fragShaderStageInfo{};
        fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        fragShaderStageInfo.module = fragShaderModule;
        fragShaderStageInfo.pName = "main";


        VkPipelineShaderStageCreateInfo shaderStages[] = { vertShaderStageInfo, fragShaderStageInfo };




        // =========================================== fixed functions =================================================





        // VkPipelineVertexInputStateCreateInfo는 vertex shader에 보내질 vertex의 format을 기술함
            // 1. Bindings: 데이터간의 spacing, 정점별 데이터인지 \
            // 인스턴스별 데이터인지(geometry instancing: 똑같은 mesh를 하나의 화면에 여러개 그리는 것 => 나뭇잎, 관절(팔))
            // 2. Attribute discription: vertex shader로 보내지는 attribute의 type, 바인딩을 로드하기 위한 오프셋
        VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
        vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vertexInputInfo.vertexBindingDescriptionCount = 0;
        vertexInputInfo.pVertexBindingDescriptions = nullptr; // Optional
        vertexInputInfo.vertexAttributeDescriptionCount = 0;
        vertexInputInfo.pVertexAttributeDescriptions = nullptr; // Optional
        // pVertexBindingDescriptions 랑 pVertexAttributeDescriptions멤버는 struct array에 대한 포인터이다.
        // 그리고 앞서 말했던 것처럼 load할 vertex data들의 detail에 대한 정보를 담고있다.


        // VkPipelineInputAssemblyStateCreateInfo는 2개의 정보를 기술하는 struct이다
            // 1. 어떤 형태의 geometry가 그려질 것인지 (topology 멤버변수)
                // point, line, line_strip, triangle, tirangle_strip
                // strip은 vertex를 연속적으로 그려서 vertex의 정보를 아끼는 것
            // 2. primitive restart를 활성화 할지
                // restart를 VK_TRUE로 enable하면 element buffer를 사용 가능하다.
                // 라인이나 triangle을 쪼개서 _STRIP 모드와 0xFFFF 혹은 0xFFFFFFFF 같은 index를 특별한 인덱스를 통해 다시 그릴 수 있다.

        VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
        inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        inputAssembly.primitiveRestartEnable = VK_FALSE;

        // viewport: output이 렌더링 될 frameBuffer의 영역. 대부분 (0, 0)에서 (width, height) 까지의 영역을 커버
        VkViewport viewport{};
        viewport.x = 0;
        viewport.y = 0;
        viewport.width = (float)swapChainExtent.width; // 스왑체인의 사이즈랑 window의 사이즈가 다를 수 있다는 것을 알고있자
        viewport.height = (float)swapChainExtent.height;
        viewport.minDepth = 0.0f; // minDepth랑 maxDepth는 frameBuffer에서 사용될 range를 표현한다. 둘 다 0.0~1.0 사이여야 한다.
        viewport.maxDepth = 1.0f;

        // scissor: viewport가 image에서 frambuffer로 transformation을 정의한다면 scissor는 어느 pixel의 region이 실제로
        // 저장될지에 대해 정의한다. scissor밖의 rectangle들은 rasterizer에 의해 사라지며 transformation보다는 function
        // 과 같은 역할을 한다.
        VkRect2D scissor{};
        scissor.offset = { 0,0 };
        scissor.extent = swapChainExtent;

        // viewport랑 scissor는 모두 static/dynamic state가 모두 될 수 있다.
        // static이 구현할 때 좀 편하기는 하지만 유연성을 위해 dynamic으로 두는게 종종 편하다. 성능 저하도 없다.
        VkPipelineViewportStateCreateInfo viewportState{};
        viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        viewportState.viewportCount = 1;
        viewportState.pViewports = &viewport;
        viewportState.scissorCount = 1;
        viewportState.pScissors = &scissor;


        // depth testing, face culling, scissor test와 같은 설정 가능
        VkPipelineRasterizationStateCreateInfo rasterizer{};
        rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        rasterizer.depthClampEnable = VK_FALSE;
        // true로 설정하면 near/far plan너머에 있는 fragment들이 discard되지 않고 clamp됨, 섀도우맵 등에 활용하기에 좋음
        rasterizer.rasterizerDiscardEnable = VK_FALSE;
        // true면 geometry정보가 rasterize stage에 넘어가지 않음. framebuffer에 대한 output을 전부 disable함
        rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
        // _FILL: polygon의 fragment를 채움, _LINE: polygon의 edge가 line으로 그려짐, _POINT: polygon의 edge가 point로 그려짐
        rasterizer.lineWidth = 1.0f;
        // line의 width를 말한다. maximum line width는 기기마다 다르고 1.0보다 굵은 라인은 wideLine GPU feature가 enable돼야 한다.
        rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
        // back face culling을 사용할지 
        rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;
        // front face인지 검사하기 위한 vertex order의 순서를 정의함. clockwise와 counterclockwise가 있음

        rasterizer.depthBiasEnable = VK_FALSE;
        rasterizer.depthBiasConstantFactor = 0.0f; // Optional
        rasterizer.depthBiasClamp = 0.0f; // Optional
        rasterizer.depthBiasSlopeFactor = 0.0f; // Optional
        // shadow mapping을 위해 bias나 slope를 정해줄 수 있지만 지금은 안쓰니까 패스


        // multi sampling을 지원함, anti-aliasing용,
        // multiple polygons의 프래그먼트 셰이더 결과를 결합하여 사용 가능
        // 보통 edge를 따라서 작동
        VkPipelineMultisampleStateCreateInfo multisampling{};
        multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        multisampling.sampleShadingEnable = VK_FALSE;
        multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
        multisampling.minSampleShading = 1.0f; //optional
        multisampling.pSampleMask = nullptr; //optional
        multisampling.alphaToCoverageEnable = VK_FALSE; //optional
        multisampling.alphaToOneEnable = VK_FALSE;  //optional


        // VkPipelineDepthStencilStateCreateInfo를 통해 depth/stancil buffer도 설정할 수 있지만 지금은 안쓸것이므로 패스

        // Color blending
        // fragment shader결과가 나온 이후 color들은 combine돼야 할 필요가 있다. color blending에는 두 종류가 있다.
            // 1. old color랑 new color를 섞는다.
            // 2. bitwise operation을 이용해 old color와 new color를 조합한다.
        VkPipelineColorBlendAttachmentState colorBlendAttachment{}; // framebuffer마다 붙는 configuration
        colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT
            | VK_COLOR_COMPONENT_A_BIT;
        colorBlendAttachment.blendEnable = VK_FALSE;
        // blend enable이 false이면 그냥 새로운 컬러가 덮어씌워지는 형식으로 rendering 된다.
        colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
        colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
        colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
        colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
        colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;
        // 위와 같이 blend color를 조합 가능하다. 이를 코드로 본다면
        /*if (blendEnable) {
            finalColor.rgb = (srcColorBlendFactor * newColor.rgb) < colorBlendOp > (dstColorBlendFactor * oldColor.rgb);
            finalColor.a = (srcAlphaBlendFactor * newColor.a) < alphaBlendOp > (dstAlphaBlendFactor * oldColor.a);
        }
        else {
            finalColor = newColor;
        }

        finalColor = finalColor & colorWriteMask;

        */ // 이와 유사한 느낌이다.
        // 실제 어떤 channel만 pass through할지 결정하기 위해 colorWriteMask를 bitwise 연산 해준다. (어떤 bitwise인지는 blendstateinfo에 저장됨
        // https://registry.khronos.org/vulkan/specs/1.3-extensions/man/html/VkBlendFactor.html => 실제 enum의 의미들

        VkPipelineColorBlendStateCreateInfo colorBlending{};
        colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        colorBlending.logicOpEnable = VK_FALSE;
        // framebuffer별로 blend attachment state를 지정 가능하며 지정한 연산을 실제 수행해 주기 위해선 logicOnEnable을 VK_TRUE로 줘야한다.
        colorBlending.logicOp = VK_LOGIC_OP_COPY;
        // 함수의 마지막 bitwise operation을 정해준다.
        // blend 모두를 전부 disable하면 수정되지 않은채로 값이 framebuffer에 올라온다.
        colorBlending.attachmentCount = 1;
        colorBlending.pAttachments = &colorBlendAttachment;
        colorBlending.blendConstants[0] = 0.0f;
        colorBlending.blendConstants[1] = 0.0f;
        colorBlending.blendConstants[2] = 0.0f;
        colorBlending.blendConstants[3] = 0.0f;


        // VkPipelineLayout 오브젝트를 이용하면 텍스쳐 샘플러 변수나 transformation matrix같은 
        // uniform variable들을 specify하는데 사용할 수 있다.
        // 일단 지금은 사용하지 않을 것이므로 pipelineLayout변수가 비어있도록 pipelineLayoutCreateInfo를 설정해
        // 비어있는 pipelineLayout 로컬 변수를 정의해줄 것이다.
        VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
        pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutInfo.setLayoutCount = 0;
        pipelineLayoutInfo.pSetLayouts = nullptr;
        pipelineLayoutInfo.pushConstantRangeCount = 0;
        pipelineLayoutInfo.pPushConstantRanges = nullptr;



        // 오래된 그래픽 api들은 그래픽파이프라인의 대부분의 stage를 default로 제공했다.
        // 그러나, vulkan은 파이프라인을 immutable(불변하는)한 pipeline state object로 만들어야 한다.
        // 대부분의 파이프라인 state는 변경 불가능하지만 viewport size, line width, blend constant등등은 변경 가능하다.
        // 이를 위해선 VkPipelineDynamicStateCreateInfo를 써야한다.
        std::vector<VkDynamicState> dynamicStates = {
            VK_DYNAMIC_STATE_VIEWPORT,
            VK_DYNAMIC_STATE_SCISSOR
        };


        VkPipelineDynamicStateCreateInfo dynamicState{};
        dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
        dynamicState.pDynamicStates = dynamicStates.data();
        // 이렇게 세팅하면 viewport랑 scissor state가 flexible해지면서도 complex해짐

        // 일단 지금은 사용하지 않을 것이므로 pipelineLayout변수가 비어있도록 pipelineLayoutCreateInfo를 설정해
        // 비어있는 pipelineLayout 로컬 변수를 정의해줄 것이다.
        if (vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS) {
            throw std::runtime_error("failed to create pipeline layout!");
        }
        // 위처럼 fixed functions기반의 파이프라인을 설정하면 unexpected behavior가 생기는 것을
        // 방지할 수 있다.
        // ================================== fixed functions (end) ============================================

        // pipeline을 만들기 전에, 우리는 vulkan에게 렌더링과정에서 사용될 framebuffer attachment가 무엇이 있는지 
        // 알려야 한다. 얼마나 많은 color/depth buffer가 있는지, 얼마나 많은 샘플들이 사용될지
        // 어떻게 rendering operation을 통해 이들의 contents가 관리돼야 하는지를 기술해야 한다.
        // 그리고 이러한 모든 과정들은 render pass object에 warpping 돼있다.


        
        VkGraphicsPipelineCreateInfo pipelineInfo{};
        pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        pipelineInfo.stageCount = 2;
        pipelineInfo.pStages = shaderStages;

        pipelineInfo.pVertexInputState = &vertexInputInfo;
        pipelineInfo.pInputAssemblyState = &inputAssembly;
        pipelineInfo.pViewportState = &viewportState;
        pipelineInfo.pRasterizationState = &rasterizer;
        pipelineInfo.pMultisampleState = &multisampling;
        pipelineInfo.pDepthStencilState = nullptr; // Optional
        pipelineInfo.pColorBlendState = &colorBlending;
        pipelineInfo.pDynamicState = &dynamicState;

        pipelineInfo.layout = pipelineLayout;

        pipelineInfo.renderPass = renderPass;
        pipelineInfo.subpass = 0;
        // 해당 파이프라인이 어떤 렌더패스를 쓰고 거기서도 어떤 서브패스를 사용할지 결정
        // 이 파이프라인은 렌더패스와 in, out 폼이 호환돼야 함

        pipelineInfo.basePipelineHandle = VK_NULL_HANDLE; // Optional
        pipelineInfo.basePipelineIndex = -1; // Optional
        // 파이프라인은 이미 존재하는 파이프라인에서 derived된 파이프라인을 생성 할 수 있다(상속처럼)
        // 기능에 많은 공통점이 있을 때 파이프라인을 셋업하고 교체하는 과정이 저렴해집니다.
        // basePipelineHandle으로 핸들을 넘겨줘서 생성할 수도 있고 basePipelineIndex를 이용해 인덱스로
        // 생성하려고 하는 다른 파이프라인을 참조할 수도 있습니다.
        // VkGraphicsPipelineCreateInfo에서 VK_PIPELINE_CREATE_DERIVATIVE_BIT플래그가 활성화 돼있으면 기능 사용 가능


        if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &graphicsPipeline) != VK_SUCCESS) {
            throw std::runtime_error("failed to create graphics pipeline!");
        }
        // vkCreateGraphicsPipelines함수는 multiple파이프라인을 생성하는 것이 목표라 파라미터가 좀 더 많음
        // VK_NULL_HANDLE 인자가 들어간 두 번째 파라미터는 VkPipelineCache오브젝트를 레퍼런스함
        // 파이프라인 캐시는 파이프라인 생성에 관한 데이터 재사용/저장에 사용될 수 있고 심지어는 캐시가 파일로
        // 저장되면 프로그램 넘어서도 도움을 줄 수 있음.
        // 이건 파이프라인 생성속도를 상당히 높여줄 수 있음


        vkDestroyShaderModule(device, fragShaderModule, nullptr);
        vkDestroyShaderModule(device, vertShaderModule, nullptr);
        // SPIR-V byte code를 GPU에 맞는 machine code로 바꾸기 위해선 파이프라인이 만들어져야 한다.
        // 그 말인 즉슨, 파이프라인이 만들어지면 이미 machine code가 생겨 shaderModule은 필요가 없어진다. 
    }

    VkShaderModule createShaderModule(const std::vector<char>& code) {
        VkShaderModuleCreateInfo createInfo{};

        createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        createInfo.codeSize = code.size();
        createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());

        VkShaderModule shaderModule;
        if (vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
            throw std::runtime_error("failed to create shader module!");
        }
        // 셰이더 모듈이 만들어지면 spir-v에서 빌드된 셰이더코드가 저장된 버퍼는 바로 해제돼도 된다.

        return shaderModule;
    }

    void cleanupSwapChain() {
        for (size_t i = 0; i < swapChainFrameBuffers.size(); i++) {
            vkDestroyFramebuffer(device, swapChainFrameBuffers[i], nullptr);
        }

        for (size_t i = 0; i < swapChainImageViews.size(); i++) {
            vkDestroyImageView(device, swapChainImageViews[i], nullptr);
        }

        vkDestroySwapchainKHR(device, swapChain, nullptr);
    }


    // Chapter: Drawing a triangle -> Swap Chain recreation
    // 지금까지 우리가 만든 어플리케이션은 triangle을 완벽하게 그릴 수 있습니다.
    // 그러나, 적절히 다룰 수 없는 예외적인 상황이 몇 가지 있습니다.
    // window surface가 swap chain자체를 바꿔버린 다던가하는 상황이 오면 더 이상 호환이 불가능합니다.
    // 가령, 윈도우의 사이즈를 바꿔버린다던가 하는 상황같은 거죠.
    // 이번에는 이러한 이벤트를 잡아내서 스왑체인을 다시 만드는 과정을 시연할 겁니다.
    //
    // recreateSwapChain()함수를 만들고 createSwapChain함수랑 스왑체인이나 윈도우 사이즈에 영향을 받는
    // 함수들을 전부 여기서 호출해보죠
    //
    void recreateSwapChain() {

        int width = 0, height = 0;
        glfwGetFramebufferSize(window, &width, &height);
        while (width == 0 || height == 0) {
            glfwGetFramebufferSize(window, &width, &height);
            glfwWaitEvents();
        }
        // 처음의 glfwGetFramebufferSize함수는 윈도우 사이즈가 타당한 경우에 무조건 실행되고
        // glfwWaitEvetns는 기다릴 것이 없는 경우를 처리합니다.
        // 축하합니다! 우리는 이제서야 제대로 작동하는 vulkan프로그램을 만들어냈습니다!
        // 다음챕터부터는 vertex셰이더 내에 하드코딩된 정점데이터를 없애고 실제로 vertex buffer를 사용해보도록 합시다!

        vkDeviceWaitIdle(device);
        // 우선 VkDeviceWaitIdle함수를 호출해줍시다. 저번 챕터의 마지막 부분에서 말했었죠? 우리는 사용하고 있는 리소스들에 대해서 직접 접근하면 안된다고.
        // 그렇기에 현재 진행중인 작업들 즉, 리소스의 사용이 끝나기를 기다려야 합니다.

        cleanupSwapChain();

        createSwapChain();
        createImageViews();
        createFrameBuffers();
        // 다음으론 우리는 스왑체인 자체를 다시 만들어줘야 합니다.
        // 이미지뷰도 다시 만들어야 합니다. 왜냐면 이미지뷰는 스왑체인 이미지에 기반하니까요
        // 마지막으로, 프레임버퍼는 직접적으로 스왑체인 이미지에 의존하기에 다시 만들어줘야 합니다.

        // 그런데  이전 버전의 오브젝트들이 새로 만들어지기 전에 이전 버전의 리소스들을 전부 해제 해줘야 하기 때문에
        // 우리는 cleanup 코드의 일부를 실제 객체를 생성하기 전에 호출해줘야 합니다. 위부분에 cleanupSwapChain 함수를
        // 정의하고 호출해주도록 하죠

        // 근데 여기서, 지금은 렌더패스를 다시 만들어주고있지는 않다는 것에 주목하도록 합시다.
        // 이론적으로는 스왑체인 이미지 포맷 자체가 어플리케이션의 life time동안 변화하는 것이 가능하거든요?
        // 가령 윈도우를 기본 모니터에서 HDR모니터로 옮긴다거나 하면요.
        // 이건 어플리케이션이 dynamic range의 변화가 잘 반영되도록 렌더패스를 다시 만드는 것을 필요로 할 수도 있습니다.
        //

        // 이제 swapChain refresh의 부분으로써 다시 만들어져야 하는 모든 오브젝트들에 대한 cleanup 코드를
        // cleanup 함수에서 cleanupSwapChain함수로 옮겨주도록 합시다.
        //

        // 여기서 chooseSwapExtent함수가 이미 (resize된)새로운 window resolution에 대해 쿼리를 진행한다는 것을 알아야 합니다.
        // 저번에 구현했듯이 createSwapChain함수 자체가 chooseSwapExtent함수를 호출해 extent 사이즈를 알려주잖아요?
        // 그래서 chooseSwpaExtent함수를 수정하거나 할 필요는 없습니다.
        // 
        // 이게 스왑체인을 다시 만들기 위한 일들의 전부입니다! 그러나, 이러한 접근 방법의 단점은 새로운
        // 스왑 체인을 만들기 전에 우리가 모든 렌더링 과정을 멈춰야 한다는 거겠죠.
        // 새로운 스왑체인을 얻어오기 전의 스왑체인 이미지로부터 드로잉 커맨드를 실행하면서
        // 새로운 스왑체인을 만드는 것도 가능은 합니다. 
        // 오래된 스왑체인을 VkSwapchainCreateInfoKHR구조체의 oldSwapChain필드에 복사해서 넣어주고
        // 스왑체인의 사용이 끝나자마자 이걸 파괴해버리면 되죠
        // 
        // 
        // 이제 우리는 언제 스왑체인을 재생성 하고 재생성 함수를 호출해야 하는지에 대해 알아보아야 합니다.
        // 운이 좋게도, Vulkan은 보통 presentatio동안에 스왑체인이 더 이상 적절하지 않다고 알려줍니다.
        // vkAcquireNextImageKHR 함수랑 vkQueuePresentKHR 함수는 다음과 같은 특별한 값을 리턴하곤 하죠.
        // 
        // VK_ERROR_OUT_OF_DATE_KHR: 스왑체인이 surface와 호환되지 않고 렌더링 과정에서 사용될 수 없습니다.
        // 보통 윈도우를 resize하면 일어납니다.
        // VK_SUBOPTIMAL_KHR: 스왑체인이 surface에 완벽하게 present 할 수 있습니다. 그러나 surface properties가 
        // 더 이상 정확히 매치하진 않습니다.
        // drawFrame함수의 vkAcquireNextImageKHR 함수 호출 부분으로 가볼까요?
        //

    }

    void createImageViews() {
        // imageView는 말 그대로 image에 대한 view이다.
        // 이미지 뷰는 어떻게 이미지에 접근하고 어느 이미지에 접근할지를 정의해준다.
        // 예를 들어 밉맵없는 2D뎁스맵 텍스쳐로 다뤄져야 하는 경우처럼 말이다.

        swapChainImageViews.resize(swapChainImages.size());

        for (size_t i = 0; i < swapChainImages.size(); i++) {
            VkImageViewCreateInfo createInfo{};
            createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            createInfo.image = swapChainImages[i];

            createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
            createInfo.format = swapChainImageFormat;


            // component field는 swizzling이 가능하게 해준다.
            //swizzling: 그래픽스에서 swizzling은 다른 벡터의 구성 요소를 임의로 재배열하고 결합하여 벡터를 구성하는 기능
            // 가령, A = {1,2,3,4}일때 B = A.wwxy 면 B={4,4,1,2} 이다.
            createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
            createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
            createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
            createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;

            // subresourceRange필드는 이미지를 사용하는 목표, 이미지의 어느 부분이 접근 가능한지 등을 기술한다. 
            createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;;
            createInfo.subresourceRange.baseMipLevel = 0;
            createInfo.subresourceRange.levelCount = 1;
            createInfo.subresourceRange.baseArrayLayer = 0;
            createInfo.subresourceRange.layerCount = 1;
            // stereographic 3D application을 만들기 위해선 multiple layer의 swap chain을 생성해야 한다.
            // 그러면 왼쪽, 오른쪽 눈에 대응되는 multiple image view를 만들어낼 수 있다.

            if (vkCreateImageView(device, &createInfo, nullptr, &swapChainImageViews[i]) != VK_SUCCESS) {
                throw std::runtime_error("failed to create image views!");
            }
            
        }

        // 지금 설정까지 하면 imageView를 texture로 만들기엔 충분하다.
        // 하지만 아직 redner target으로 만들려면 아직이다.

    }

    void createSwapChain() {
        SwapChainSupportDetails swapChainSupport = querySwapChainSupport(physicalDevice);

        VkSurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(swapChainSupport.formats);
        //surface format에 대한 설명은 chooseSwapSurfaceFormat에 달려있음
        // surface는 window창과 대응되는 개념
        VkPresentModeKHR presentMode = chooseSwapPresentMode(swapChainSupport.presentModes);
        VkExtent2D extent = choosSwapExtent(swapChainSupport.capabilities);


        uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1;
        // image가 min만큼만 있으면 driver가 internal operation을 완료할 때까지 기다려야만 렌더할 image를 얻어올 수 있기 때문에
        // 적어도 minimage보다 많아야 한다. 


        if (swapChainSupport.capabilities.maxImageCount > 0 && imageCount > swapChainSupport.capabilities.maxImageCount) {
            imageCount = swapChainSupport.capabilities.maxImageCount;
        }
        // 그리고 imageCount가 maxImagecount보다 적게 만들어야 한다. 근데, maxImageCount == 0 이면 maximum이 정해져 있지 않는다는
        // 뜻이기 때문에 이에 대한 예외처리가 필요하다. 

        VkSwapchainCreateInfoKHR createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        createInfo.surface = surface;

        createInfo.minImageCount = imageCount;
        createInfo.imageFormat = surfaceFormat.format;
        createInfo.imageColorSpace = surfaceFormat.colorSpace;
        createInfo.imageExtent = extent;
        createInfo.imageArrayLayers = 1;
        // 각각의 이미지를 구성하는 레이어의 개수, stereoscopy 3D 기술을 쓰지 않으면 항상 1이다.
        createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        // imsageUsage는 스왑체인에 무슨 종류의 operation을 사용할지 고르는 것이다.
        // VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT는 스왑체인의 이미지에 바로 그리는 것을 의미하며 color attachment라고 한다.
        // 포스트 프로세싱같은 작동을 위해 별도의 이미지에 이미지를 렌더링 하는 것도 가능하며 이를 위해선
        // VK_IMAGE_USAGE_TRANSFER_DST_BIT를 대신 사용하면 된다.

        QueueFamilyIndices indices = findQueueFamilies(physicalDevice);
        uint32_t queueFamilyIndiecs[] = { indices.graphicsFamily.value(), indices.presentFamily.value() };

        if (indices.graphicsFamily != indices.presentFamily) {
            createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
            // 명시적으로 ownership을 transfer하지 않아도 다른 큐에서 사용 가능하다.
            // 해당 옵션을 위해선 사전에 어느 큐 패밀리 ownership이 공유될지를 미리 설정해야 한다.
            // 해당 모드는 일반적으로 두개 이상의 큐 패밀리를 가진 디바이스에서 필요로된다.
            createInfo.queueFamilyIndexCount = 2;
            createInfo.pQueueFamilyIndices = queueFamilyIndiecs;
        }
        else {
            createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
            // 큐 패밀리가 가진 이미지는 명시적으로 transfer돼야 다른 큐에서 사용 가능하게 한다.
            // 성능적으로 효율이 좋다.
            createInfo.queueFamilyIndexCount = 0; //optional
            createInfo.pQueueFamilyIndices = nullptr; //optional
        }

        createInfo.preTransform = swapChainSupport.capabilities.currentTransform;
        // 스왑체인의 이미지에 특별한 transform이 들어가야 하는지를 알려줌.(ex. 시계방향으로 90도 회전)
        // 필요 없으면 currentTransform으로 설정

        createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        // 다른window system에서 window랑 blending하는데 알파채널이 사용되는지를 특정함. 대부분 무시할거기 때문에 위와 같이 설정

        createInfo.presentMode = presentMode;
        createInfo.clipped = VK_TRUE;
        // 다른 윈도우 창에 가려서 clipping된 픽셀의 색은 신경쓰지 않는 모드
        // 가려진 픽셀을 읽어 결과를 예측하는 경우가 아니면 활성화하여 성능 향상 가능

        createInfo.oldSwapchain = VK_NULL_HANDLE;
        // 화면이 resize되거나 하는 등의 이유로 swap chain이 비활성되거나 최적화되지 못하는 경우가 있음
        // 이런 경우에 스왑체인은 이전 버전의 스왑체인을 참고해 새로이 만들어져야 함.
        // 일단 지금은 스왑체인 하나만 만드는 형태이기때문에 스킵


        if (vkCreateSwapchainKHR(device, &createInfo, nullptr, &swapChain) != VK_SUCCESS) {
            throw std::runtime_error("failed to create swap chain!");
        }
        // 차례로 디바이스, createInfo, 전용 allocator, swapchin
        // 전용 allocator는 본인이 원하는 대로 swapchain의 create함수를 정의 가능

        
        vkGetSwapchainImagesKHR(device, swapChain, &imageCount, nullptr);
        // 만들어진 swap chain을 기반으로 swap chain image들 생성
        swapChainImages.resize(imageCount);
        vkGetSwapchainImagesKHR(device, swapChain, &imageCount, swapChainImages.data());

        swapChainImageFormat = surfaceFormat.format;

        swapChainExtent = extent;

    }

    void createSurface() {
        if (glfwCreateWindowSurface(instance, window, nullptr, &surface) != VK_SUCCESS) {
            throw std::runtime_error("failed to create window surface");
        }
    }

    struct SwapChainSupportDetails {
        VkSurfaceCapabilitiesKHR capabilities;
        std::vector<VkSurfaceFormatKHR> formats;
        // 어떤 포맷으로 색을 저장하는지와 ex)R8G8B8, 그리고 어떤 colorSpace를 가졌는지에 대해 저장하는 구조체 ex) LINEAR_EXT
        // chooseSwapSurfaceFormat 함수에 자세한 설명 달려있음.
        std::vector<VkPresentModeKHR> presentModes;
        // MAILBOX_KHR, FIFO_KHR 등의 present mode를 정의하는 enum
    };

    SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device) {
        SwapChainSupportDetails details;

        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &details.capabilities);
        // surface와 physicalDevice로부터 가능한 기능들을 불러온다.
        // 모든 support query function들은 device와 surface를 파라미터로 가진다. 스왑체인에서 매우 중요하기 때문이다.

        uint32_t formatCount;
        vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, nullptr);

        if (formatCount!= 0) {
            details.formats.resize(formatCount);
            vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, details.formats.data());
        } // 어떤 포맷으로 색을 저장하는지 ex)R8G8B8, 그리고 어떤 colorSpace를 가졌는지 저장 ex) LINEAR_EXT

        uint32_t presentModeCount;
        vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, nullptr);

        if (presentModeCount != 0) {
            details.presentModes.resize(presentModeCount);
            vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, details.presentModes.data());
        }// MAILBOX_KHR, FIFO_KHR 등의 present mode를 정의하는 enum

        return details;
    }

    void createLogicalDevice() {
        QueueFamilyIndices indices = findQueueFamilies(physicalDevice);

        std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
        std::set<uint32_t> uniqueQueueFamilies = { indices.graphicsFamily.value(), indices.graphicsFamily.value() };
        float queuePriority = 1.0f;

        for (uint32_t queueFamily : uniqueQueueFamilies) {
            VkDeviceQueueCreateInfo queueCreateInfo{};
            queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            queueCreateInfo.queueFamilyIndex = queueFamily;
            queueCreateInfo.queueCount = 1;
            queueCreateInfo.pQueuePriorities = &queuePriority;
            queueCreateInfos.push_back(queueCreateInfo);
        }

        VkPhysicalDeviceFeatures deviceFeatures{};
        

        VkDeviceCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
        createInfo.pQueueCreateInfos = queueCreateInfos.data();

        createInfo.pEnabledFeatures = &deviceFeatures;

        createInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
        createInfo.ppEnabledExtensionNames = deviceExtensions.data();

        if (enableValidationlayers) {
            createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
            createInfo.ppEnabledLayerNames = validationLayers.data();
        }
        else {
            createInfo.enabledLayerCount = 0;
        }
        // 이전 vulkan의 구현에서는 instance와 validation layer간의 구분을 뒀었다.
        // 그런데, RayTracing으로 인해 이제 그것은 더 이상 구분이 되지 않는다.
        // 그 말인 즉슨, enabledLayerCount와 ppEnabledLayerNames는 현재 무시되고 있는 상황이다.
        // 그러나, 예전 구현 표준에 맞춰 이것을 어떻게든 초기화 하는 것은 아직도 추천되고 있는 상황이다.

        if (vkCreateDevice(physicalDevice, &createInfo, nullptr, &device) != VK_SUCCESS) {
            throw std::runtime_error("failed to create logical device!");
        }

        vkGetDeviceQueue(device, indices.graphicsFamily.value(), 0, &graphicsQueue);
        vkGetDeviceQueue(device, indices.presentFamily.value(), 0, &presentQueue);

    }

    void createInstance() {

        if (enableValidationlayers && !checkValidationLayerSupport()) {
            throw std::runtime_error("validation layers requested, but not available");
        }


        VkApplicationInfo appInfo{};
        appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        appInfo.pApplicationName = "Hello Triangle";
        appInfo.applicationVersion = VK_MAKE_VERSION(1, 3, 236);
        appInfo.pEngineName = "No Engine";
        appInfo.engineVersion = VK_MAKE_VERSION(1, 3, 236);
        appInfo.apiVersion = VK_API_VERSION_1_3;
        // applicaton info를 만든다. 이는 기술적으로는 설정하지 않아도 되지만
        // driver에게 최적화에 관련된 유용한 정보들을 넘겨준다.

        VkInstanceCreateInfo createInfo{}; // createInfo는 필수이다.
        createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        createInfo.pApplicationInfo = &appInfo; //appInfo는 createInfo에 담아서 전달된다.

        
        auto extensions = getRequiredExtensions();
        // validation layer가 enabled돼있는지에 따라서 extensions의 list를 받아옴

        createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
        createInfo.ppEnabledExtensionNames = extensions.data();
        // glfw를 포함해 해당 어플에 필요로되는 익스텐션이 무엇이 있는지 목록을 만들어
        // 해당 데이터를 포함해 vkInstance를 만든다.



        VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};
        // debug messenger에는 문제가 하나 있다. 바로 instance가 생성된 이후에 debug messenger가 생성돼야 하기 때문에
        // instance생성에 대한 debug는 불가능 하다는 것이다. 이를 위해서 instance를 생성할 때, DebugMessengerCreateInfo를 선언하고
        // instance생성용 createInfo의 pNext필드에 해당 debugCreateInfo를 미리 전해줘서 create할 때의 debug log를 전송받을 수 있다.
        if (enableValidationlayers) {
            createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
            createInfo.ppEnabledLayerNames = validationLayers.data();

            populateDebugMessengerCreateInfo(debugCreateInfo);
            createInfo.pNext = (VkDebugUtilsMessengerCreateInfoEXT*)&debugCreateInfo;
        }
        else {
            createInfo.enabledLayerCount = 0;

            createInfo.pNext = nullptr;
        }


        if (vkCreateInstance(&createInfo, nullptr, &instance) != VK_SUCCESS) {
            throw std::runtime_error("failed to create instance!");
        }



    }

    void populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo) {
        createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT; 
        createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
        createInfo.pfnUserCallback = debugCallback;
        // 어떤 타입의 메세지를 만들고 콜백함수로는 뭘 지정할지에 대한 정보를 담는 함수

        // 추가로 pUserData필드를 추가 할 수 있다.
        // 해당 필드는 callback함수에서 정의한 pUserData를 통해 얻어온 데이터를 어떤 변수가 받을지에 대한 포인터이다.
    }

    void setupDebugMessanger() {
        if (!enableValidationlayers) {
            return;
        }

        VkDebugUtilsMessengerCreateInfoEXT createInfo{};
        populateDebugMessengerCreateInfo(createInfo);
        // 어떤 debug messenger를 보낼지에 대한 struct

        // vulkan에선, debugmessenger마저 handle을 이용해 명시적으로 생성해주고 파괴해줘야 한다.
        // 이를 위해서 class멤버로 debugMessenger를 선언해줘야 한다.
        if (CreateDeubgUtilMessengerEXT(instance, &createInfo, nullptr, &debugMessenger) != VK_SUCCESS) {
            throw std::runtime_error("failed to set up debug messenger!");
        }
        
    }

    void pickPhysicalDevice() {
        uint32_t deviceCount = 0;
        vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);

        if (deviceCount == 0) {
            throw std::runtime_error("failed to find GPUs with Vulkan support!");
        }

      

        std::vector<VkPhysicalDevice> devices(deviceCount);
        vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());
        

        for (const auto& device : devices) {

            if (isDeviceSuitable(device)) {
                physicalDevice = device;

                VkPhysicalDeviceProperties deviceProp;
                vkGetPhysicalDeviceProperties(physicalDevice, &deviceProp);
                std::cout<< "Selected device: " << deviceProp.deviceName << "\n";

                break;
            }
        }

        if (physicalDevice == VK_NULL_HANDLE) {
            throw std::runtime_error("failed to find a suitable GPU!");
        }

    }



    struct QueueFamilyIndices {
        std::optional<uint32_t> graphicsFamily;
        std::optional<uint32_t> presentFamily;
        // drawing command와 presentation의 큐 패밀리는 겹치지 않기 때문에 
        // 별도의 기능을 하는 새로운 큐를 만들 필요가 있다.

        bool isComplete() {

            return graphicsFamily.has_value() && presentFamily.has_value();
        }
        //Queue Family에 대한 설명
        //https://lifeisforu.tistory.com/404
    };

    QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device) {
        QueueFamilyIndices indices;

        uint32_t queueFamilyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

        std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

        int i = 0;


        for (const auto& queueFamily : queueFamilies) {


            if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
                indices.graphicsFamily = i;
            }

            VkBool32 presentSupport = false;
            vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presentSupport);

            if (presentSupport) {
                indices.presentFamily = i;
            }


            if (indices.isComplete()) {
                break;
            }



            i++;
        }


        return indices;

    }


    bool isDeviceSuitable(VkPhysicalDevice device) {
        QueueFamilyIndices indices = findQueueFamilies(device);


        bool extensionsSupported = checkDeviceExtensionSupport(device);

        bool swapChainAdequate = false;
        if (extensionsSupported) {
            SwapChainSupportDetails swapChainSupport = querySwapChainSupport(device);
            swapChainAdequate = !swapChainSupport.formats.empty() && !swapChainSupport.presentModes.empty();
        }

        return indices.isComplete() && extensionsSupported && swapChainAdequate;
    }

    bool checkDeviceExtensionSupport(VkPhysicalDevice device) {
        uint32_t extensionCount;
        vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);

        std::vector<VkExtensionProperties> availableExtensions(extensionCount);
        vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());

        std::set<std::string> requiredExtensions(deviceExtensions.begin(), deviceExtensions.end());

        for (const auto& extension : availableExtensions) {
            requiredExtensions.erase(extension.extensionName);
        }

        return requiredExtensions.empty();
    }


    // swapChainImageFormat
    // 각각의 VkSurfaceFormatKHR 요소들은 format과 colorSpace 멤버를 포함한다.
    // format 은 컬러 채널과 타입을 특정한다. VK_FORMAT_B8G8R8A8_SRGB라면 8bit uint로 RGBA를 저장한다는 뜻이다. 
    // colorSpace는 국제 규격인 SRGB color space가 지원되는지를 VK_COLOR_SPACE_SRGB_NONLINEAR_KHR 플래그로 확인 할 수 있다.
    // 해당 플래그는 올드버전에서 VK_COLORSPACE_SRGB_NONLINEAR_KHR 라고 불렸다.
    VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats) {

        for (const auto& availableFormats : availableFormats) {

            if (availableFormats.format == VK_FORMAT_B8G8R8A8_SRGB &&
                availableFormats.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
                return availableFormats;

            }
        }

        return availableFormats[0];

    }
    // SRGB와 linear RGB의 차이 => 인같은 어두운 환경을 인지하는 능력이 더 뛰어나기 때문에 선형으로 빛의 밝기를 표현하면
    // 구분하지도 못할 밝은 영역의 빛의 intensity는 촘촘하게 구성되고 구분하기 용이한 어두운 영역의 빛의 intensity는 rought하게 된다
    // 이를 위해 비트의 구성을 변형하여 빛의 세기를 표현하는 것이 SRGB의 요지이다.



    VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes) {

        for (const auto& availablePresentMode : availablePresentModes) {
            if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR) {
                return availablePresentMode;
            }
        }
        //presentation mode는 총 4개가 존재한다.
        // VK_PRESENT_MODE_IMMEDIATE_KHR: app에서 계산된 이미지가 바로 스크린에 전달됨. 티어링이 생길 수 있음
        // VK_PRESENT_MODE_FIFO_KHR: fifo형태의 큐에 이미지를 쌓아놓음.
        //          큐가 가득차면 프로그램이 기다림. v-sync와 가장 유사함. 화면이 refresh되는 순간을 vertical blank라고 부름
        // VK_PRESENT_MODE_FIFO_RELAXED_KHR: 위의 모드와 유사. 다른점이라면 어플의 계산이 늦어지고 마지막 vertical blank에서
        //          큐가 비어있으면 다음 vertical blank를 기다려서 동기화 하는 것이 아닌 현재 렌더링하고 있는 이미지가 바로
        //          그려짐. visible tearing을 만들어냄
        // VK_PRESENT_MODE_MAILBOX_KHR: 두 번째 모드와 유사. 차이점으론 큐가 가득 찼을 때 블락을 거는 것이 아닌 
        //          큐의 마지막 이미지가 덮어쓰기됨. 티어링 방지가 가능하고 렌더링을 빨리 할 수 있어 수직동기화보다 latency
        //          이슈가 적게 일어남. 흔히 트리플 버퍼링이라고 불림. 근데 실제 버퍼가 3개 있다고 프레임속도의 제한이 풀리는 것은 아님


        // VK_PRESENT_MODE_MAILBOX_KHR 모드는 에너지 효율을 생각하지 않으면 가장 좋은 옵션임
        // VK_PRESENT_MODE_FIFO_KHR 모드는 모바일 디바이스처럼 에너지 효율이 필요로 될 때 사용하기 좋음
        return VK_PRESENT_MODE_FIFO_KHR;
    }

    VkExtent2D choosSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities) {
    // SwapChain의 extent는 swapchain의 resolution을 의미하며 이는 보통 DPI가 높은 모니터를 사용하지 않는 이상
    // 모니터의 resolution과 똑같음. 
        if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
            return capabilities.currentExtent;
        }
        else {
            int width, height;
            glfwGetFramebufferSize(window, &width, &height);
            // glfw는 window 사이즈를 잴 때, screen coordinate랑 pixel 두 가지 unit을 제공함

            VkExtent2D actualExtent = {
                static_cast<uint32_t>(width),
                static_cast<uint32_t>(height)
            };

            actualExtent.width = std::clamp(actualExtent.width, capabilities.minImageExtent.width,
                capabilities.maxImageExtent.width);
            
            actualExtent.height = std::clamp(actualExtent.height, capabilities.minImageExtent.height,
                capabilities.maxImageExtent.height);

            return actualExtent;

        }

        
    }


#pragma endregion


#pragma region render loop

    // semaphore와 fence에 대한 설명 

    // high level단에서 vulkan은 common set of step으로 작동합니다.
    // 1. 이전 프레임이 끝나기를 기다린다.
    // 2. swap chain으로부터 이미지를 얻어온다.
    // 3. 스왑체인으로부터 얻어온 이미지에 그림을 그릴 command buffer를 기록한다.
    // 4. 기록된 command buffer를 전송한다.
    // 5. 스왑체인 이미지를 present 한다.
    //
    // vulkan의 주요한 디자인 철학 중 하나는 바로 GPU에서의 동기화가 명시적이라는 것입니다.
    // 실행순서는 드라이버에게 실행시키고 싶은 순서를 알려주는 다양한 동기화 요소들을 우리가 어떻게 정의하냐에 달렸습니다.
    // 이 말인 즉슨, vulkan에서 작동하는 API call들은 동작이 완료되기 전에 리턴되는 비동기적인 동작방식을 가졌다는 것입니다. 
    //      이번 챕터에서 우리가 명시적으로 순서를 정해야 하는 event들이 있습니다.
    //      1) 이미지를 스왑체인으로부터 얻어오기
    //      2) 얻어온 이미지에서 draw command를 실행하기
    //      3) 이미지를 스크린에 present하고 다시 스왑체인에 반납하기
    // 해당 명령들은 단일 function call이지만 비동기적입니다. 함수의 호출은 실제 동작이 되기 전에 끝나며 실행 순서는 정의되지 않습니다.
    // 각각의 operation들이 이전 동작에 종속적이라는 것은 매우 안타까운 일이죠. 그러므로 우리는 우리가 원하는 실행 순서를
    // 위해 어느 요소들을 사용 가능한지에 대해 살펴봅니다.  
    //
    // 1. 세마포어
    // 세마포어는 queue operation 사이의 순서를 정해주기 위해 사용됩니다. 
    // queue operation은 명령 함수 내부에서 혹은 커맨드 버퍼로부터 큐에 보내지는 작업들을 의미합니다. 이 부분은 나중에 살펴보겠습니다.
    // 큐의 주된 예시로는 그래픽스 큐와 프레젠트 큐가 있습니다. 세마포어는 같은 큐 내부의 작업을 정렬하거나 
    // 큐들 사이의 작업순서를 정렬하기 위해 사용됩니다. 
    // 
    // vulkan에선 두 가지의 세마포어가 있습니다. 바이너리와 타임라인입니다. 이번 예제에선 바이너리 세마포어만 활용해 보도록 하죠
    // 사실은 타임라인에 대해선 이야기를 꺼낼 생각이 아직 없기에 그냥 앞으로 세마포어라 하면 그냥 바이너리 세마포어라고 생각해주시면
    // 편하겠습니다.
    // 
    // 세마포어는 signaled이거나 unsignaled입니다. 이게 무슨 의미냐? (signaled는 사용가능, unsignaled는 사용 불가능)
    // 모든 세마포어는 unsignaled로 시작합니다. 그리고 queue operation을 정렬하기 위해 세마포어를 사용하는 방법은 같은 세마포어를
    // signal으로 둠으로써 다른 queue operation을 기다리게 하는 것입니다. 운영체제 등을 통해 세마포어에 이미 익숙하신 분들은
    // 무슨 뜻인지 이미 이해 하셨겠죠? 하지만 모르는 사람도 있을 수 있을테니 구체적인 예시를 들어보도록 하죠.
    // 세마포어 S와 우리가 실행하고하 자는 동작(operation) A, B가 있습니다.
    // 우리가 Vulkan에게 알려줘야 하는 것은 A동작이 끝났을 때 세마포어 S에게 시그널을 보낸다는 것과 
    // 동작 B는 실행을 시작하기 전에 세마포어 S에서 기다려야 한다는 것입니다.
    // 
    // 동작 A가 끝나면, 세마포어 S는 시그널을 받고 B는 세마포어 S가 시그널을 받을 때까지 시작이 불가능하다는 것입니다.
    // B가 시작되면, S는 자동적으로 시그널되지 않은 상태로 들어갑니다. 다시 사용되기를 기다리면서요.
    // 그래요. 맞습니다. A가 세마포어한테 "얼음!"하면 S는 A가 다시 "땡!"해주기 전까지 얼어있어야 해요. B는 S가 "땡!"될 때까지 기다려야 하구요.
    // 
    // 2. fence
    // fence는 동기화를 한다는 관점에서 본다면 같은 목적을 가지고 있습니다. 그러나, 이건 CPU(host)에서의 실행 순서의 정렬을 위한 겁니다. 
    // 다시말해, CPU(host)가 GPU의 작업이 언제 끝났는지를 알고 싶으면 우리는 fence를 써야 합니다.
    // 세마포어와 유사하게, 펜스 또한 signaled/unsignaled상태를 지닙니다. 우리가 실행할 작업을 보낼 때마다, 우리는 작업에 fence를
    // 붙일 수 있습니다. 작업이 끝나면, 펜스에 시그널이 보내집니다. 그러면 우리는 host가 펜스가 시그널을 받을 때까지 기다리도록
    // 할 수 있습니다. 그러면 호스트가 보내진 작업이 끝났는지를 보장할 수 있게 되지요.
    // 
    // 구체적인 예시는 스크린샷 작업이 될 수 있습니다. GPU에서 필요한 작업이 다 끝났다고 생각해보죠. 이제 GPU에서 얻은 이미지를
    // CPU(host)로 전송하고 파일을 메모리에 저장해야 합니다. 우리는 전송작업을 수행하는 커맨드 버퍼 A와 펜스F를 가지고 있습니다.
    // 우리는 커맨드 버퍼A에 펜스F를 첨부해 전송합니다. 그리고 즉시 host(CPU)에게 F를 기다리라고 말합니다. 이렇게 되면
    // host는 커맨드 버퍼A가 끝날 때까지 기다려야 하지요. 그러면 우리는 호스트가 파일을 디스크에 안전하게 저장하도록 할 수 있습니다!
    // 
    // 세마포어와 다르게, 위의 예제의 vkWaitForFence(F) 는 host가 실행되는 것을 막습니다. 그 말인 즉슨, 호스트는 A의 실행이 끝날 때까지
    // 아무것도 하지 않는다는 겁니다! 그래서 보통 세마포어를 선호하고 아직 언급하지 않은 다른 동기화 기법을 선호합니다.
    // 그리고, 펜스는 무조건 직접 리셋돼야 합니다! 왜냐면 펜스는 호스트가 작업하는걸 통제하고, 때문에 호스트는 언제 펜스를
    // 재설정할지 결정하기 때문입니다. 호스트의 개입없이 GPU간의 작업순서를 결정하는 세마포어와는 대조되는거죠.
    // 
    // 짧게 말해, 세마포어는 GPU에서의 작업 순서를 결정하고 펜스는 GPU, CPU간의 작업 순서를 결정합니다.
    // 
    // 3. 뭘 써야하나요?
    // 우리는 두 개의 동기화 요소에 대해 배웠고 이 두 가지 요소는 두 가지 동기화 작업을 편리하게 수행 가능케 해줍니다!(헉, 엄청난 우연이네요)
    // 바로 스왑체인 동작과 이전 프레임의 작업이 끝나기를 기다리는 것입니다.
    // 스왑체인 작업에선 세마포어를 쓸겁니다! 왜냐면 스왑체인 작업은 GPU에서 일어나고 그래서 우리는 가능한
    // host가 작업을 기다리게 두지는 않고 싶으니까요.
    // 이전 프레임의 작업이 끝나기를 기다리기 위해선 펜스를 사용할 겁니다! 왜냐면 우리는 호스트가 기다리기를 원하니까요
    // 이게 우리가 한 번에 하나 이상의 프레임을 그리지 않는 이유죠.
    // 우리는 매 프레임마다 해야 할 작업들을 재정렬 해야 하기 때문에 우리는 이번 프레임이 끝날 때까지 다음 프레임의 작업을
    // 커맨드 버퍼에 기록할 수 없습니다. 왜냐면 우리는 GPU가 사용하고 있는 현재의 커맨드 버퍼에 내용들을 덮어쓰고 싶진 않으니까요.
    //


    void mainLoop() {

        while (!glfwWindowShouldClose(window)) {
            glfwPollEvents();
            drawFrame();
        }
        
        vkDeviceWaitIdle(device);
        // 이러한 부류의 함수들은 아주 기초적으로 동기화를 실행하기 위해 실행되는 함수들이죠.
        // 이제 프로그램이 윈도우를 아무 문제없이 닫아내는 것을 볼 수 있습니다!


        // 900줄이 넘는 코드를 입력하고 나서야, 우리는 겨우겨우 스크린에 무언가를 띄워냈네요!
        // vulkan program을 부트스트래핑(더 복잡하고 빠른 환경을 구성)하는 작업은 분명히 더 많은 작업이 필요하지만
        // vulkan은 이러한 명시성을 통해 상당한 양의 control을 줄 것입니다. 
        // 코드를 한 번 다시 읽어보고 모든 vulkan object에 대한 개념을 다시 빌드해보세요 
        // 우리는 지금 시점부터 기능성을 확장하기 위해 지금까지 배운 모든 지식들을 빌드할거거든요
        // 다음 챕터에선, 비행(작동)중인 여러 프레임을 처리하기 위해 루프를 확장할겁니다.
        //


        // Chapter: Drawing a triangle -> Drawing -> Frames in flight
        // 이제 우리의 렌더 루프에는 눈에 띄는 결함이 하나 있습니다. 보통 렌더루프에서 CPU와 GPU의 exploitation을
        // 최대화 하기 위해서 GPU에서 프레임을 렌더하는 작업과 CPU에서 드로우콜을 보내는 작업을 비동기적으로 작업하거든요?
        // 그걸 위해서 우리는  CPU에서 드로우 콜을 보내는 "해당 프레임"이 완료되길 기다리는 것이 아닌
        // 드로우 콜을 보냄과 동시에 "이전 프레임"의 GPU에서의 그리기 작업을 동시에 진행할 겁니다.
        // 
        // 이러한 문제점을 해결하기 위해서 여러 프레임을 한 번에 in-flight(비행하도록) 하는 겁니다.
        // 다시 말해서, 하나의 프레임을 그리는 과정이 다음의 recording 과정을 방해하지 못하도록 하는 겁니다.
        // 이걸 어떻게 할까요? 이를 위해선 한 프레임의 렌더링 과정에서 접근되고 수정되는 모든 리소스들은 반드시 복사돼야 합니다.
        // 또한 여러개의 프레임 버퍼, 세마포어, 펜스가 필요하죠. 나중 챕터에서 다른 리소스들에 대한 여러 인스턴스들을
        // 추가할 것이기 때문에 이 개념이 다시 나타날 겁니다.
        // 
        // 얼마나 많은 프레임들이 동시에 렌더링 될지에 대한 const 변수를 선언하면서 시작해보죠 
        // private 멤버변수로 const int MAX_FRAMES_IN_FLIGHT = 2; 를 추가해줍시다.
        // 
        // 우리는 2라는 숫자를 선택했습니다. 왜냐면 CPU가 GPU보다 너무 앞서가길 원하진 않거든요.
        // 한 번에 2프레임만 진행하게 두면서 CPU랑 GPU는 각각의 작업을 동시에 진행할 수 있게 됩니다.
        // 또한, 만약 CPU가 생각해놨던 2프레임 보다 일찍 끝나면 해야 할 일을 GPU에 보내기 전에
        // GPU의 작업이 끝나길 기다릴 거에요.
        // 3개 이상의 frame들이 같이 진행되게 둔다면 CPU가 GPU를 너무 앞서서 프레임 지연이 생길 수도 있습니다.
        // 일반적으로 추가적인 지연은 추천되는 사항은 아니구요
        // 그치만 동시 진행 가능한 프레임 숫자를 직접 지정 가능케 하는 것도 Vulkan의 명시성에 대한 또 다른 예라고 볼 수 있겠네요
        // 
        // 각각의 프레임은 스스로의 커맨드 버퍼, 세마포어, 펜스를 가져야돼요. 함수와 변수의 형태를 바꾸도록 합시다.
        // 
        // 
        // 우리는 여러 커맨드 버퍼를 만드는 것을 명시적으로 보여주기 위해위의 createCommandBuffer 멤버 함수를 
        // createCommandBuffers로 이름을 바꿉시다.
        // 그리고 위에 멤버로 선언해 놨었던 commandBuffer 변수를 벡터형태의 commandBuffers로 만듭시다.
        // 
        // 이제 실제 createCommandBuffers함수로 가서, 실제 커맨드 버퍼를 생성하는 부분들의 코드를 다음과 같이 바꿔줍시다.
        // 
        // 이후 createSyncObjects 함수로 가서 semaphore와 fence관련 멤버 변수들의 생성 또한 다시 해줍니다.
        // 
        // cleanUp 함수로 가서 객체를 지우는 것도 잊지 말도록 합시다.
        // 맞다, 전에도 말했지만 커맨드 버퍼는 destroy 해줄 필요가 없습니다. commandPool이 dstroy될때 자동으로 같이 되니까요.
        // 
        // 이젠 매 프레임마다 올바른 오브젝트를 사용하기 위해서 current frame을 tracking 해줄겁니다. 멤버변수로
        // uint32_t currentFrame = 0; 을 선언해 줍시다. 그리고 drawFrame함수로 가서 해당 변수를 실제로 사용하기 위해
        // 다음과 같이 함수를 수정해 줍시다.
        // 
        // 함수의 마지막엔 당연히 프레임이 끝났으면 매 번 다음 프레임값으로 갱신해주는 것도 잊으면 안되겠죠 
        // 
        // 이제 MAX_FRAMES_IN_FLIGHT개의 프레임보다 적은 수의 프레임 작업들이 queue에 들어가고 각 프레임들이
        // 서로를 짖뭉게지 못하도록하는 모든 필요한 동기화 작업을 완료했습니다.
        // 
        // 여기서 주의할 점이 하나 있습니다. 최종적으로 했던 cleanUp() 함수와 같은 코드들처럼 다른 부분들은
        // vkDeviceWaitIdle함수처럼 대략적으로 동기화 함수들에 의존하는 것이 더 좋습니다.
        // 성능 요구사항에 따라서 어떤 접근방법을 사용할지는 직접 결정하세요.
        // 
        // 동기화에 대해 더욱 배우고 싶다면 다음의 링크를 참고하세요
        // https://github.com/KhronosGroup/Vulkan-Docs/wiki/Synchronization-Examples#swapchain-image-acquire-and-present
        // 
        // 다음 챕터에선, 잘 작동하는 vulkan프로그램을 위해 필요로되는 작은 부분에 대해 하나 더 다뤄볼 겁니다.
        // 
        // 
        // 
        // 

    }

    void drawFrame() {

        // 드디어 drawFrame의 시작입니다! 지금까지는 전부 그리기를 위한 사전 작업이었고 드디어 그리기를 시작합니다.
        // 지금까지만 해도 정말 힘겨운 여정이었는데 이제 겨우 시작이라니... 하지만 걱정마세요. 
        // 원래 모든 일이든 시작이 반이라고 하잖아요. 실제로 Vulkan의 경우에도 하드웨어 가속을 위한 최적화를 위해
        // 가능한 하드웨어의 기능들을 모두 외부로 노출했기에 사전작업이 복잡했을 뿐이지 실제 렌더링 작업으로 가면
        // 생각보다 별 일 없습니다. 아마도...요?
        
        vkWaitForFences(device, 1, &inFlightFences[currentFrame], VK_TRUE, UINT64_MAX);
        // 우선, 우리는 두 개의 프레임이 동시에 렌더링 되길 원하지 않기에 그리기를 시작하기 전에 
        // 펜스를 이용해 이전 프레임이 끝날 때까지 기다려 주도록 하겠습니다.
        // 만약 그리려고 할 때 이전 프레임의 렌더링이 이미 끝났으면 기다리지 않고 바로 넘어가겠죠.
        // vkWaintForFences는 하나의 펜스만을 기다리는 것이 아닌 여러개의 펜스를 기다리기 위해 만들어진 함수인데
        // fence의 array를 받을 수 있습니다. 두 번째 파라미터는 array의 size 세 번째 파라미터는 array의 주소입니다.
        // 네 번째의 VK_TRUE는 우리가 array의 모든 펜스를 기다리겠다는 뜻입니다. 지금의 경우에는 어차피 fence가 한개니
        // 큰 의미는 없겠네요.
        // 해당 함수는 또한, fence를 기다리는 최대치인 timeout을 정할 수 있는데 이를 UINT64_MAX로 지정하면 timeout을 없앨 수 있죠
        
        // vkQueuePresentKHR함수의 result를 통해 reacreateSwapChain을 했더라도 여전히 남아있는 문제가 있습니다.
        // 지금같은 경우에 데드락이 걸릴 수도 있거든요? 코드를 디버깅 해보면 어플리케이션이 vkWaitForFences함수에서 데드락이 걸리는
        // 것을 볼 수 있습니다.
        // 왜냐면 vkAcquireNextImageKHR의 반환값이 VK_ERROR_OUT_OF_DATE_KHR라면 스왑체인을 다시 만들고 drawFrame을 리턴하기
        // 때문입니다. 하지만 지금같은 경우 리턴이 일어나기 전에 현재 프레임의 실행을 vkWaitForFences로 기다리고 바로 
        // vkResetFences를 하잖아요? 그러면 펜스가 unsignaled될텐데 함수를 중간에 리턴해 버렸기 때문에
        // GPU에게 작업을 보내주지 않아서 다시 signaled상태로 바꿔줄 작업의 목록을 보내주지 않아서 평생 unsignaled상태에
        // 멈춰버리는 것이죠.
        // 
        // 지금 상황에서 해결방법은 간단합니다. vkResetFences함수를 스왑체인 recreate조건을 판단하는 로직 아래로 내려버리면
        // queue에 작업을 보내는 것과 펜스를 unsignaled상태로 두는 작업단위가 한 번에 실행되도록 보장이 되겠죠
        // 이렇게 해서, 만약 return이 일찍 일어나도 펜스는 여전히 signaled상태라서 같은 펜스 오브젝트를 활용하는 다음 시간까지
        // 데드락이 걸리는 일은 없겠죠
        // 
        //

        // 펜스 설정을 모두 마친 이후 drawFrame 내에서 해야 할 다음 일은, 스왑체인으로부터 이미지를 얻어오는 것입니다.
        // swapChain은 현재 glfw로부터 얻어온 extension이기에 vk*KHR함수를 이용하도록 합니다. 
        uint32_t imageIndex;
        VkResult result = vkAcquireNextImageKHR(device, swapChain, UINT64_MAX, imageAvailableSemaphores[currentFrame], VK_NULL_HANDLE, &imageIndex);
        // 첫번째랑 두번째 파라미터는 뭔지 다들 아실테고, 세 번째 파라미터는 나노세컨드 단위로 이미지가 available해지는
        // 것을 기다리는 timeout입니다. MAX로 설정해서 일단은 비활성화 해둡시다.
        // 다음 두 파라미터는 present engine이 이미지를 사용하는 것을 끝냈을 때 어떤 semaphore에게 신호를 줄 지 입니다.
        // 세마포어도 가능하고 펜스도 가능합니다. 지금은 세마포어를 이용할 것이므로 imageAvailableSemaphore를 네번째 파라미터로 전달해줍니다.
        // 마지막 파라미터는 사용가능해진 이미지의 인덱스를 얻어옵니다. 해당 인덱스는 통해 우리가 클래스 멤버로 만들어둔
        // swapChainImages 배열의 인덱스에 해당합니다. 우리는 VkFrameBuffer를 골라주기 위해 해당 인덱스를 활용하겠습니다.

        if (result == VK_ERROR_OUT_OF_DATE_KHR) {
            recreateSwapChain();
            return;
        }
        else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
            throw std::runtime_error("failed to acquire swap chain image!");
        }
        // 만약 이미지를 얻어오려고 하는데 스왑체인이 out of date상태라면, 해당 스왑체인에 present하는 것은 불가능합니다.
        // 그러므로 스왑체인을 다시 만들어서 다름 drawFrame 호출에 다시 시도해야 합니다.
        // 
        // 또한 suboptimal인지도 확인해야겠죠? 하지만 저는 어쨋든 suboptimal상태에도 그냥 present를 계속 진행하도록 하겠습니다.
        // 왜냐면 우리는 이미 이미지를 얻어왔잖아요? VK_SUCCESS와 VK_SUBOPTIMAL_KHR모두 success로 간주하도록 합시다.
        // 
        // 이제 해당 함수의 vkQueuePresentKHR 함수의 호출부로 이동해보도록 하죠
        //


        vkResetFences(device, 1, &inFlightFences[currentFrame]);
        // fence를 기다리는 것이 끝났다면 이전 프레임의 렌더링이 완료됐다는 뜻이므로, 이젠 이번 프레임을 그리기 시작해야 함으로
        // 이번 프레임을 위한 펜스를 새롭게 지정해줍시다. 근데, 작업을 계속하기 전에, 우리 코드에 좀 기겁할만한 점이 있습니다.
        // 우리가 처음 drawFrame() 을 호출할 때, 곧바로 inFlightFence가 signaled되기를 기다립니다.
        // inFlightFence는 렌더링이 끝난 이후에 signaled되기 때문에 제일 처음이 화면에 뜨기 시작하는 그 시점에는 signal을 해줄
        // 이전 프레임이 없습니다! vkFWaitForFences함수는 절대 일어나지 않을 일을 기다리며 바보같이 멀뚱멀뚱 서있겠죠
        // 
        // 이러한 딜레마의 해결방법은 빌트인 API에 있습니다. fence를 처음 만들 때 signaled state로 시작하도록 만들어주면 됩니다.
        // 이를 위해, createSyncObjects()함수로 돌아가 K_FENCE_CREATE_SIGNALED_BIT를 지정해 주도록 합시다.
        //





        // imageIndex를 얻어온 이후, command buffer에 해야 할 일들을 기록하겠습니다.
        // 우선, vkResetCommandBuffer 함수를 불러 기록이 가능하게 해줍니다.
        vkResetCommandBuffer(commandBuffers[currentFrame], 0);
        // 두 번째 파라미터는 VkCommandBufferResetFlagBits 라는 flag인데 지금은 딱히 특별한 설정을 해주지 않을거라 0으로 남깁니다.
        // 이제, recordCommandBuffer를 이용해 우리가 원하는 command를 기록해줍시다.
        recordCommandBuffer(commandBuffers[currentFrame], imageIndex); // commandBuffer는 핸들값이기에 그냥 넘겨줘도 됨
        // 기록을 완료하면, 이제 커맨드 버퍼를 GPU에 전송 할 수 있습니다.
        // (해당 함수는 우리가 전에 직접 정의해준 함수입니다)

        // Queue submission과 synchronization은 VkSubmitInfo 구조체에 파라미터를 정의합니다.
        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

        VkSemaphore waitSemaphores[] = { imageAvailableSemaphores[currentFrame]};
        VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };


        submitInfo.waitSemaphoreCount = 1;
        submitInfo.pWaitSemaphores = waitSemaphores;
        submitInfo.pWaitDstStageMask = waitStages;
        // 위 세개의 파라미터는 실행이 시작하기 전에 어느 세마포어의 어느 스테이지에 파이프라인이 대기할지를 기술합니다.
        // image에 color를 쓰는 작업이 가능할 때까지 기다리고 싶기 때문에
        // VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT라고 지정해 color attachment에 쓰기를 수행하는 단계를 기다리도록 지정합니다.
        // color attachment에 쓰는 그래픽스 파이프라인의 스테이지를 기다립니다. 
        // 이 말을 이론적으로 본다면, "이미지가 아직 available하지 않아도 vertex shader는 이미시작할 수도 있다는 뜻입니다."(중요)
        // waitStages[] 의 각각의 요소들은 pWaitSemaphore와 대응되죠.
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &commandBuffers[currentFrame];
        // 다음 두 버퍼는, execution을 위해 어느 버퍼가 전송될지를 결정합니다. 우리는 우선 단순히 single command buffer
        // 만 사용하니 이것만 보내도록 하겠습니다.


        VkSemaphore signalSemaphores[] = { renderFinishedSemaphores[currentFrame]};
        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores = signalSemaphores;
        // signalSemaphoreCount랑 pSignalSemaphores파라미터는 커맨드 버퍼의 동작이 끝나면
        // 어느 세마포어에 시그널을 보낼지 정의합니다.
        // 우리의 경우에, renderFinishSemaphore를 사용합니다.

        if (vkQueueSubmit(graphicsQueue, 1, &submitInfo, inFlightFences[currentFrame]) != VK_SUCCESS) {
            throw std::runtime_error("failed to submit draw command buffer!");
        }
        // 이제 command buffer를 graphics queue로 보내줍니다.
        // 해당 함수는 submitInfo를 array로 받아올 수 있기 때문에 workload가 훨씬 클 때 효율적입니다.
        // 똑같은 VkSubmitInfo로 여러 commandBuffer를 보내줄 수도 있지만 다른 vkSubmitInfo로 여로 commandBuffer를 보내줄 수도 있는거죠
        // 마지막 inFlightFence는 커맨드 버퍼의 execution이 끝났을 때, 어느 fence에게 signal을 줄 지에 대해 정해줍니다.
        // 이를 통해서, 다음프레임에 CPU는 커맨드를 기록하기 전에 커맨드버퍼의 실행이 끝내길 기다릴겁니다!
        


        // createRenderPass()함수의 colorAttachment를 정의할 때, 렌더패스의 subpass가 자동으로 image layout transition을 
        // 하도록 지정했다는 것이 기억 나시나요?(안나면 보고오면 되죠)
        // 이러한 transition들은 subpass간의 메모리/실행 종속성을 기술하는 subpass dependency에 의해 컨트롤됩니다.
        // 근데 지금은 subpass가 하나밖에 없어서 의미가 없어보이지만, subpass 직전/직후의 이러한 transition들 또한 "암시적으로" subpass로 간주된답니다.
        // 
        // 렌더패스의 시작과 끝에서 transition을 책임지는 두 개의 built-in dependencies(subpass)가 있습니다.
        // 하지만, 렌더패스의 시작에 있는 놈은 우리가 원하는 시기에 시작된다고 보장 할 수 없습니다.
        // 아까 말했죠? "이미지가 아직 available하지 않아도 vertex shader는 이미시작할 수도 있다" 라구요
        // 때문에, 파이프라인의 시작에 transition이 일어날 것이라고 가정하고 설정을 해놨지만 
        // transition을 시작했을 때 아직 이미지를 얻지 못했을 수도 있습니다!
        // 이를 해결하기 위해 우리가 waitSemaphores와 waitStages를 설정한 것이죠 
        // waitStages를 VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT 로 정해주면
        // 이미지가 사용 가능해질때까지 렌더패스가 시작하지 않도록 정의해줄 수 있습니다!
        // 또는, 기존 설정해 뒀던 대로 VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT 스테이지까지 기다리도록 만들어도 됩니다.
        // 
        // 서브패스의 종속성과 그들이 어떻게 일하는 지에 대한 이해를 위해서 일단은 기존 설정대로 놔둡시다.
        
        // 그리고 또 문제가 있습니다. 사실 서브패스는 실행 순서가 없거든요?
        // 서브패스는 default로 특정 순서를 지켜서 실행되지 않고 동기적으로 실행됩니다.
        // 그것이 우리가 subpass dependencies를 지정해 줘야 하는 이유입니다.
        // 우리가 여기서 semaphore로 했던 작업을 subpass에선 subpass dependencies가 해결 해준다고 생각하면 되겠군요.
        
        // subpass의 종속성을 기술하는 것은 VkSubpassDependency 구조체에 정의됩니다.
        // createRenderPass()함수로 가서 dependency라는 이름의 구조체를 정의해보죠.

        // 자, 이제는 마지막 step으로 swapchain에 결과를 돌려주는 작업만 남았습니다.
        // presentation은 VkPresentInfoKHR구조체에 정의됩니다.
        VkPresentInfoKHR presentInfo{};
        presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

        presentInfo.waitSemaphoreCount = 1;
        presentInfo.pWaitSemaphores = signalSemaphores;
        // 처음 두 파라미터는 위의 submitInfo에서 정의했던 것처럼 presentation이 일어나기 전에 
        // 어느 semaphore를 기다려야 하는지를 정의합니다. 우리는 command buffer가 execution을 끝내고 나서 triangle이
        // 그려지길 원하기 때문에 signalSemaphores를 사용하도록 하겠습니다.
        
        VkSwapchainKHR swapChains[] = { swapChain };
        presentInfo.swapchainCount = 1;
        presentInfo.pSwapchains = swapChains;
        presentInfo.pImageIndices = &imageIndex;
        // 다음 두 파라미터는 이미지들을 present하기 위한 스왑체인이 무엇인지와 각각의 스왑체인의
        // 어느 이미지에 그릴지를 정의합니다. 대부분의 경우에는 하나의 스왑체인에만 그림을 그릴거에요
        
        presentInfo.pResults = nullptr; // Optional
        // 마지막은 optaional한 파라미터인데 위에서 설정해준 각각의 스왑체인에 대한 presentation결과값이
        // 성공적이었는지 아닌지를 반환해주는 value의 array를 정의해줍니다.
        // 근데 대부분의 경우에는 하나의 스왑체인을 쓰고 있기 때문에 필수적이진 않습니다.
        // 왜냐면 presentation 함수 자체가 해당 값을 리턴해주거든요. 
        
        result =  vkQueuePresentKHR(presentQueue, &presentInfo);
        // 위의 함수를 통해 이미지를 스왑체인에 present하는 것을 요청합니다.
        // 이에 대한 에러 핸들링은 vkAcquireNextImageKHR 와 vkQueuePresentKHR 에서 이뤄지는데 이는 다음 챕터에서 다뤄보죠
        // 왜냐면 우리가 봤던 다른 함수들처럼 여기서 실패한다고 필수적으로 프로그램이 종료해야하진 않거든요.
        // 만약 여기서 잘 마무리 했다면, 삼각형을 볼 수 있을거에요. 와!
        //
        // 근데, 만약 열렸던 창을 닫으면 프로그램이 크래시가 나서 validation error가 뜨는 것을 볼 수 있을거에요
        // debugCallback 함수가 왜 그런지 말해줄텐데 한 번 살펴 보도록 하죠
        // 
        // 에러 메세지 봅시다. drawFrame의 모든 동작이 비동기적이라는것이 기억 나시나요? 그건 바로 루프를 빠져나갈 때
        // drawFrame과 presentation이 비동기적이라는게 기억 나시나요? 그 말인 즉슨 mainLoop를 빠져나가면 presentation과 drawing동작은
        // 아마도 계속 작동되고 있을거라는 뜻입니다. 그런데 그렇다고 동작중에 리소스를 전부 정리하면 비효율적이겠죠?
        // 
        // 우선은 logical device가 mainLoop를 나가기 전에 operation을 완수하기를 기다리고 윈도우를 없애는게 좋습니다.
        // 이를 위해 mainLoop 함수로 돌아가보죠
        //

        if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || framebufferResized) {
            framebufferResized = false;
            recreateSwapChain();
        }
        else if (result != VK_SUCCESS) {
            throw std::runtime_error("failed to present swap chain image!");
        }
        // vkQueuePresentKHR 함수는 위의 vkAcquireNextImageKHR와 같은 값들을 리턴합니다.
        // 이러한 경우에도 우리는 만약 이것이 suboptimal이라면 스왑체인을 다시 만들어야되죠.
        // 이젠 다시 vkWaitForFences 함수로 돌아가 봅시다.




        currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
        // 당연히 프레임이 끝났으면 매 번 다음 프레임값으로 갱신해주는 것도 잊으면 안되겠죠 

    }

    // commandBuffer파라미터를 해당 함수에 패스해서 쓰기를 시작할거임
    void recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex) {


        VkCommandBufferBeginInfo beginInfo{}; // 커맨드 버퍼에 쓰기 위해선 해당 struct를 만들어 줘야함
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = 0;
        // VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT: 커맨드 버퍼의 각각의 recording은 한 번만 보내집니다. 
        // 그리고 나서 커맨드 버퍼는 각 submission간에 다시 리셋되고 기록됩니다.
        // VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT: 만약 해당 버퍼가 secondary command buffer라면
        // 커맨드 버퍼는 완전히 렌더 패스 안에 있는 것으로 인식됩니다. primary라면 무시됩니다.
        // VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT: 커맨드 버퍼는 pending state에 있을 때 큐에 재전송될
        // 수 있습니다. 그리고 여러개의 프라이머리 커맨드 버퍼에 기록될 수 있습니다.
        beginInfo.pInheritanceInfo = nullptr;
        // secondary command 버퍼만 사용할 수 있는 optional한 파라미터이며 primary command buffer에서 어떤 상태를 
        // 상속받을지를 결정합니다.

        if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
            throw std::runtime_error("failed to begin recording command buffer!");
        }

        // vkCmdBeginRenderPass로 렌더패스를 시작하면 그리기를 시작한다.
        // 렌더패스는 VkRenderPassBeginInfo구조체로 시작할 수 있다.
        VkRenderPassBeginInfo renderPassInfo{};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassInfo.renderPass = renderPass; // 어떤 렌더 패스를 이용할지 결정
        renderPassInfo.framebuffer = swapChainFrameBuffers[imageIndex];
        // 위에서 생성했던 (이미지에 이미지 뷰를 통해 바인딩된)프레임버퍼들 렌더패스에 직접 바인딩함으로써 렌더패스 시작 => 즉, 렌더 타겟 설정!
        renderPassInfo.renderArea.offset = { 0, 0 };
        renderPassInfo.renderArea.extent = swapChainExtent;
        // 셰이더가 로드되고 저장되는 위치를 정의함. 이 영역 밖의 region은 정의되지 않지만 attachment size랑 동일하게 설정하는게 best

        VkClearValue clearColor = { {{0.0f, 0.0f, 0.0f, 1.0f}} };
        //VK_ATTACHMENT_LOAD_OP_CLEAR에서 정의했던 clear operation을 위해 쓰일 것이다. => (0,0,0,1)이면 black으로 클리어 =>뒷배경이 black
        renderPassInfo.clearValueCount = 1;
        renderPassInfo.pClearValues = &clearColor;

        vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
        // 이러면 이제 렌더패스가 시작된다. command를 record하는 함수들은 전부 vkCmd prefix가 붙는다.
        // 그리고 전부 void를 리턴한다. => 실제 recording이 끝날 때까진 에러가 생기지 않기때문에
        // VK_SUBPASS_CONTENTS_INLINE: 렌더패스 커맨드가 primary command buffer에 임베드 되고 secondary command buffer는 쓰지 않음
        // VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS: 렌더 패스 command가 secondary command buffer에서 실행됨


        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);
        // 두 번째 파라미터를 통해 파이프라인이 그래픽스용인지 compute shade용인지 기술해줌
        // 파이프라인에 커맨드 버퍼를 바인딩해줌


        // 파이프라인에서 viewport랑 scissor설정을 dynamic으로 해줬기 때문에 drawcall을 내기 전에
        // 커맨드버퍼의 뷰포트 사이즈를 정의해줘야 함
        VkViewport viewport{};
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width = static_cast<float>(swapChainExtent.width);
        viewport.height = static_cast<float>(swapChainExtent.height);
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
        vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

        VkRect2D scissor{};
        scissor.offset = { 0, 0 };
        scissor.extent = swapChainExtent;
        vkCmdSetScissor(commandBuffer, 0, 1, &scissor);


        vkCmdDraw(commandBuffer, 3, 1, 0, 0);
        // vertexCount: vertex의 개수가 몇 개인지
        // instanceCount: instanced rendering을 위해 사용. 지금은 안쓰니까 1
        // firstVertex: vulkan은 opencl과 같이 spir-v를 쓰기 때문에 in변수의 이름을 지정해서
        //              데이터가 전송되는 것이 아닌 gl_vertexIndex를 이용해 직접 접근함
        // firstInstance: instanced rendering을 위해 쓰임. gl_InstanceIndex가 최소값임


        vkCmdEndRenderPass(commandBuffer);
        if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
            throw std::runtime_error("failed to record command buffer!");
        }
    }




#pragma endregion

    void cleanup() {

        cleanupSwapChain();


        vkDestroyPipeline(device, graphicsPipeline, nullptr);
        vkDestroyPipelineLayout(device, pipelineLayout, nullptr);

        vkDestroyRenderPass(device, renderPass, nullptr);

        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            vkDestroySemaphore(device, imageAvailableSemaphores[i], nullptr);
            vkDestroySemaphore(device, renderFinishedSemaphores[i], nullptr);
            vkDestroyFence(device, inFlightFences[i], nullptr);
        }

        
        // Command buffer는 cmannd buffer가 없어질 때 자동으로 없어지기 때문에 별도로 commandBuffer를 없애줄
        // 필요가 없음. 실제로 없애주는 vkDestroyCommandBuffer함수도 없음
        vkDestroyCommandPool(device, commandPool, nullptr);

        vkDestroyDevice(device, nullptr);

        if (enableValidationlayers) {
            DestroyDebugUtilsMessengerEXT(instance, debugMessenger, nullptr);
        }


        vkDestroySurfaceKHR(instance, surface, nullptr);
        vkDestroyInstance(instance, nullptr);

        glfwDestroyWindow(window);
        glfwTerminate();

    }


};

int main()
{
    HelloTriangleApplication app;

    try {
        app.run();
    }
    catch (const std::exception e) {
        std::cerr << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
