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


// https://stackoverflow.com/questions/39557141/what-is-the-difference-between-framebuffer-and-image-in-vulkan
// 공부 자료

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
    VkDebugUtilsMessengerEXT debugMessenger;
    // vulkan에선, debugmessenger마저 handle을 이용해 명시적으로 생성해주고 파괴해줘야 한다.
    // 이를 위해서 class멤버로 debugMessenger를 선언해줘야 한다.
    VkSurfaceKHR surface;
    VkSwapchainKHR swapChain;
    std::vector<VkImage> swapChainImages;
    VkFormat swapChainImageFormat;
    VkExtent2D swapChainExtent;
    std::vector<VkImageView> swapChainImageViews; // VkImageView의 각각의 element는 각각의 최종 output attachment와 대응된다.

    // 파이프라인을 통해 uniform변수의 값을 바꿔주는 등의 셰이더, vertex 데이터 접근 가능
    VkRenderPass renderPass;
    VkPipelineLayout pipelineLayout;

    std::vector<VkFramebuffer> swapChainFrameBuffers;



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
        glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

        window = glfwCreateWindow(WIDTH, HEIGHT, "Vulkan", nullptr, nullptr);

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
                swapChainImageViews[i]
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


        VkRenderPassCreateInfo renderPassInfo{};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        renderPassInfo.attachmentCount = 1; 
        renderPassInfo.pAttachments = &colorAttachment; 
        renderPassInfo.subpassCount = 1;
        renderPassInfo.pSubpasses = &subpass;

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
            createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
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


        if (swapChainSupport.capabilities.maxImageCount > 0 && imageCount > swapChainSupport.capabilities.maxImageCount) {
            imageCount = swapChainSupport.capabilities.maxImageCount;
        }

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

    void mainLoop() {

        while (!glfwWindowShouldClose(window)) {
            glfwPollEvents();
        }

    }

    void cleanup() {

        for (auto framebuffer : swapChainFrameBuffers) {
            vkDestroyFramebuffer(device, framebuffer, nullptr);
        }

        vkDestroyPipeline(device, graphicsPipeline, nullptr);
        vkDestroyPipelineLayout(device, pipelineLayout, nullptr);

        for (auto imageView : swapChainImageViews) {
            vkDestroyImageView(device, imageView, nullptr);
            // window의 image와 다르게 imageView는 명시적을 우리가 직접 생성했기 때문에 직접 지워줘야함
            

        }

        vkDestroySwapchainKHR(device, swapChain, nullptr);

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
