import { useState } from "react";
import { Text, View, StyleSheet, Button } from "react-native";
import { counterStep } from "../counter";

export default function Index() {
  const [counter, setCounter] = useState(0);

  const handleCount = () => {
    setCounter(counterStep);
  };

  return (
    <View style={styles.container}>
      <Text>Counter App</Text>
      <Text>You have pushed the button this many times: {counter}</Text>
      <Button onPress={handleCount} title="+1"></Button>
    </View>
  );
}

const styles = StyleSheet.create({
  container: {
    flex: 1,
    alignItems: "center",
    justifyContent: "center",
  },
});
