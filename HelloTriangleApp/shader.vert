#version 450

/*

Chapter: Vertex buffers -> vertex input description
    Introduction
다음 몇 챕터 동안에는, 하드코딩된 정점 셰이더의 정점 정보들을 메모리에 있는 vertex buffer로 교체할 겁니다.
우린 일단 가장 간단한 접근방법인 CPU에서 접근 가능한 버퍼를 만들고 memcpy함수를 이용해 정점정보를 직접적으로 복사해 올 겁니다.
그리고 그 다음 어떻게 스테이지된 버퍼를 고성능 V-Ram의 정점 정보로 카피하는지를 볼 겁니다.




    Vertex Shader
  정점 셰이더의 첫번째 변화는 더 이상 셰이더코드 그 자체가 정점 데이터를 포함하지 않는다는 겁니다.
정점셰이더는 “in” 키워드를 이용해 입력값을 받습니다.

“inPosition”과 “inColor”변수들은 정점의 속성입니다. 이놈들은 저번에 우리가 직접 손수 위치값과 색상값을 지정해 줬던 것처럼
vertex buffer에 정점마다 정의됩니다. 이제 셰이더 코드를 고쳤으니 다시 컴파일 해주도록 합시다!

“fragColor”처럼, layout(location = x)표기법은 각 우리가 나중에 참조할 입력들에게 인덱스를 부여합니다.
여기서, dvec3처럼 여러 슬롯을 차지하는 double형태의 64비트 벡터표현과 같은 몇몇 타입들은 기본적으로 숙지하고 있는 것이 좋을겁니다.
왜냐면 “location = x”에 대응하는 여러 슬롯들을 차지하기 때문에 location값이 2 이상 올라가야 할 수도 있거든요.
layout(location = 0) in dvec3 inPosition;
layout(location = 2) in vec3 inColor;
*location 하나당 128byte를 차지하는 것으로 보임 (vec4 기준 32bit float형이 4개면 32*4 = 128bit이기 때문)




    Vertex data
  이제 vertex data를 셰이더 코드에서 우리 프로그램의 코드로 옮겨보도록 합시다. 우리에게 벡터, 행렬과 같은 선형대수 관련 타입들을 제공하는
GLM라이브러리를 추가해서 시작해보도록 합시다. 우리는 이 타입들을 색상과 위치 벡터로 정의할 겁니다.

  다시 C스크립트의 헤더부분으로 이동해 볼까요?

*/


layout(location = 0) in vec2 inPosition;
layout(location = 1) in vec3 inColor;

layout(location = 0) out vec3 fragColor;


void main() {
    gl_Position = vec4(inPosition, 0.0, 1.0);
    fragColor = inColor;
}